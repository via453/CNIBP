"""
实时血压监测系统 (方案A - 命令行版)
功能：从 STM32 MAX30102 串口接收 PPG 数据，实时预测血压
"""

import os
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3'  # 屏蔽 TensorFlow 警告

import sys
import time
import numpy as np
import serial
import pickle
from scipy.signal import resample

# 添加项目路径
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from models import UNetDS64, MultiResUNet1D


class BPRealtimeMonitor:
    """实时血压监测器"""
    
    def __init__(self, port='COM3', baudrate=115200, model_dir='models'):
        """
        初始化
        
        参数:
            port: 串口号
            baudrate: 波特率
            model_dir: 模型目录
        """
        self.port = port
        self.baudrate = baudrate
        self.model_dir = model_dir
        
        # 采样参数
        self.sensor_rate = 100    # MAX30102 采样率 100Hz
        self.target_rate = 125    # PPG2ABP 要求 125Hz
        self.target_length = 1024 # 模型输入长度
        
        # 计算需要采集的原始样本数
        # 1024 点 @ 125Hz = 8.192 秒
        # 8.192 秒 @ 100Hz = 819.2 ≈ 820 点
        self.raw_length = int(self.target_length * self.sensor_rate / self.target_rate)
        
        # 模型和数据
        self.mdl1 = None  # 近似网络
        self.mdl2 = None  # 细化网络
        self.meta = None  # 元数据
        self.serial_conn = None
        
    def load_models(self):
        """加载预训练模型"""
        print("[1/3] 加载模型...")
        
        # 加载元数据
        meta_path = os.path.join(os.path.dirname(__file__), 'data', 'meta9.p')
        if os.path.exists(meta_path):
            self.meta = pickle.load(open(meta_path, 'rb'))
            print(f"  ✓ 元数据已加载")
        else:
            print(f"  ✗ 找不到 {meta_path}")
            return False
        
        # 加载近似网络
        self.mdl1 = UNetDS64(self.target_length)
        model1_path = os.path.join(self.model_dir, 'ApproximateNetwork.h5')
        if os.path.exists(model1_path):
            self.mdl1.load_weights(model1_path)
            print(f"  ✓ 近似网络已加载")
        else:
            print(f"  ✗ 找不到 {model1_path}")
            return False
        
        # 加载细化网络
        self.mdl2 = MultiResUNet1D(self.target_length)
        model2_path = os.path.join(self.model_dir, 'RefinementNetwork.h5')
        if os.path.exists(model2_path):
            self.mdl2.load_weights(model2_path)
            print(f"  ✓ 细化网络已加载")
        else:
            print(f"  ✗ 找不到 {model2_path}")
            return False
        
        return True
    
    def connect_serial(self):
        """连接串口"""
        print(f"[2/3] 连接串口 {self.port}...")
        try:
            self.serial_conn = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=1
            )
            time.sleep(2)  # 等待 STM32 重启
            print(f"  ✓ 已连接 ({self.baudrate} baud)")
            return True
        except Exception as e:
            print(f"  ✗ 连接失败: {e}")
            return False
    
    def read_ppg_raw(self):
        """
        读取原始 PPG 数据 (约 820 个点)
        
        返回:
            numpy array: 原始 PPG 数据
            None: 读取失败
        """
        ppg_data = []
        print(f"\r\n  采集数据中 ({self.raw_length} 点, 约 {self.raw_length/self.sensor_rate:.1f} 秒)...", end='')
        
        while len(ppg_data) < self.raw_length:
            try:
                line = self.serial_conn.readline().decode('utf-8').strip()
                if line:
                    try:
                        value = float(line)
                        ppg_data.append(value)
                        # 进度显示
                        if len(ppg_data) % 100 == 0:
                            print(f"\r  采集进度: {len(ppg_data)}/{self.raw_length}", end='')
                    except ValueError:
                        continue
            except Exception:
                continue
        
        print(f"\r  ✓ 采集完成: {len(ppg_data)} 点")
        return np.array(ppg_data, dtype=np.float32)
    
    def resample_ppg(self, ppg_raw):
        """
        重采样: 100Hz → 125Hz
        
        参数:
            ppg_raw: 原始 PPG 数据 (100Hz)
            
        返回:
            重采样后的 PPG 数据 (125Hz, 1024 点)
        """
        ppg_resampled = resample(ppg_raw, self.target_length)
        return ppg_resampled.astype(np.float32)
    
    def predict_bp(self, ppg_signal):
        """
        预测血压
        
        参数:
            ppg_signal: PPG 信号 (125Hz, 1024 点)
            
        返回:
            (sbp, dbp, map_val): 收缩压, 舒张压, 平均动脉压
        """
        # 归一化到 [0, 1]
        ppg_norm = ppg_signal.copy()
        ppg_norm -= ppg_norm.min()
        ppg_range = ppg_norm.max() - ppg_norm.min()
        if ppg_range > 0:
            ppg_norm /= ppg_range
        
        # 调整形状为模型输入格式 (1, 1024, 1)
        ppg_input = ppg_norm.reshape(1, self.target_length, 1)
        
        # 第一阶段：近似网络
        abp_approx = self.mdl1.predict(ppg_input, verbose=0)
        if isinstance(abp_approx, list):
            abp_approx = abp_approx[0]  # 取主输出
        
        # 第二阶段：细化网络
        abp_refined = self.mdl2.predict(abp_approx, verbose=0)
        if isinstance(abp_refined, list):
            abp_refined = abp_refined[0]
        
        # 反归一化（转换回 mmHg）
        abp_waveform = abp_refined.ravel() * (self.meta['max_abp'] - self.meta['min_abp']) + self.meta['min_abp']
        
        # 计算血压值
        sbp = np.max(abp_waveform)      # 收缩压
        dbp = np.min(abp_waveform)      # 舒张压
        map_val = np.mean(abp_waveform) # 平均动脉压
        
        return sbp, dbp, map_val
    
    def get_bp_status(self, sbp, dbp):
        """获取血压状态"""
        if sbp < 120 and dbp < 80:
            return "正常 ✓"
        elif sbp < 130 and dbp < 85:
            return "正常偏高 △"
        elif sbp < 140 and dbp < 90:
            return "高血压前期 ⚠"
        elif sbp < 160 and dbp < 100:
            return "1级高血压 ✗"
        else:
            return "2级高血压 ✗✗"
    
    def display_result(self, sbp, dbp, map_val):
        """显示结果"""
        status = self.get_bp_status(sbp, dbp)
        now = time.strftime('%H:%M:%S')
        
        print(f"""
  ┌─────────────────────────────────┐
  │  [{now}] 血压测量结果            │
  ├─────────────────────────────────┤
  │  收缩压 (SBP): {sbp:>6.1f} mmHg      │
  │  舒张压 (DBP): {dbp:>6.1f} mmHg      │
  │  平均压 (MAP): {map_val:>6.1f} mmHg      │
  │  状态: {status:<20} │
  └─────────────────────────────────┘""")
    
    def run(self):
        """运行主循环"""
        print("=" * 50)
        print("  PPG2ABP 实时血压监测系统")
        print("=" * 50)
        
        # 加载模型
        if not self.load_models():
            return
        
        # 连接串口
        if not self.connect_serial():
            return
        
        print(f"[3/3] 系统就绪")
        print(f"\n  采样率: {self.sensor_rate}Hz → {self.target_rate}Hz (重采样)")
        print(f"  采集时长: {self.raw_length/self.sensor_rate:.1f} 秒/次")
        print(f"  按 Ctrl+C 退出\n")
        
        # 主循环
        try:
            while True:
                # 1. 采集原始 PPG 数据
                ppg_raw = self.read_ppg_raw()
                if ppg_raw is None:
                    continue
                
                # 2. 重采样 100Hz → 125Hz
                ppg_resampled = self.resample_ppg(ppg_raw)
                
                # 3. 预测血压
                print(f"  预测中...", end='')
                sbp, dbp, map_val = self.predict_bp(ppg_resampled)
                print(f"\r", end='')
                
                # 4. 显示结果
                self.display_result(sbp, dbp, map_val)
                
        except KeyboardInterrupt:
            print("\n\n  程序已退出")
        finally:
            if self.serial_conn:
                self.serial_conn.close()
                print("  串口已关闭")


def main():
    """主函数"""
    import argparse
    
    parser = argparse.ArgumentParser(description='PPG2ABP 实时血压监测')
    parser.add_argument('--port', type=str, default='COM3', help='串口号 (默认: COM3)')
    parser.add_argument('--baudrate', type=int, default=115200, help='波特率 (默认: 115200)')
    parser.add_argument('--model', type=str, default='models', help='模型目录 (默认: models)')
    
    args = parser.parse_args()
    
    monitor = BPRealtimeMonitor(
        port=args.port,
        baudrate=args.baudrate,
        model_dir=args.model
    )
    monitor.run()


if __name__ == '__main__':
    main()

"""
PPG2ABP 实时血压监测 Web 仪表盘
功能：Flask + Socket.IO 实时推送血压数据到浏览器
"""

import os
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3'

import sys
import time
import json
import threading
import numpy as np
import serial
import pickle
from scipy.signal import resample
from flask import Flask, render_template
from flask_socketio import SocketIO

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from models import UNetDS64, MultiResUNet1D

app = Flask(__name__)
app.config['SECRET_KEY'] = 'ppg2abp'
socketio = SocketIO(app, cors_allowed_origins="*")

# 全局状态
monitor_state = {
    'running': False,
    'connected': False,
    'port': 'COM37',
    'serial_conn': None,
    'mdl1': None,
    'mdl2': None,
    'meta': None,
    'bp_history': []  # 历史血压记录
}

# 采样参数
SENSOR_RATE = 100
TARGET_RATE = 125
TARGET_LENGTH = 1024
RAW_LENGTH = int(TARGET_LENGTH * SENSOR_RATE / TARGET_RATE)  # 820


def load_models():
    """加载模型"""
    base_dir = os.path.dirname(os.path.abspath(__file__))
    
    meta_path = os.path.join(base_dir, 'data', 'meta9.p')
    if os.path.exists(meta_path):
        monitor_state['meta'] = pickle.load(open(meta_path, 'rb'))
    
    model_dir = os.path.join(base_dir, 'models')
    
    monitor_state['mdl1'] = UNetDS64(TARGET_LENGTH)
    monitor_state['mdl1'].load_weights(os.path.join(model_dir, 'ApproximateNetwork.h5'))
    
    monitor_state['mdl2'] = MultiResUNet1D(TARGET_LENGTH)
    monitor_state['mdl2'].load_weights(os.path.join(model_dir, 'RefinementNetwork.h5'))
    
    print("✓ 模型加载完成")


def connect_serial(port):
    """连接串口"""
    try:
        monitor_state['serial_conn'] = serial.Serial(port, 115200, timeout=1)
        time.sleep(2)
        monitor_state['connected'] = True
        monitor_state['port'] = port
        print(f"✓ 已连接 {port}")
        return True
    except Exception as e:
        print(f"✗ 连接失败: {e}")
        return False


def read_ppg_raw():
    """读取原始 PPG 数据"""
    ser = monitor_state['serial_conn']
    ppg_data = []
    
    while len(ppg_data) < RAW_LENGTH:
        try:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                try:
                    value = float(line)
                    ppg_data.append(value)
                except ValueError:
                    continue
        except Exception:
            continue
    
    return np.array(ppg_data, dtype=np.float32)


def predict_bp(ppg_raw):
    """预测血压"""
    # 重采样 100Hz → 125Hz
    ppg_resampled = resample(ppg_raw, TARGET_LENGTH).astype(np.float32)
    
    # 归一化
    ppg_norm = ppg_resampled.copy()
    ppg_norm -= ppg_norm.min()
    ppg_range = ppg_norm.max() - ppg_norm.min()
    if ppg_range > 0:
        ppg_norm /= ppg_range
    
    # 模型推理
    ppg_input = ppg_norm.reshape(1, TARGET_LENGTH, 1)
    
    abp_approx = monitor_state['mdl1'].predict(ppg_input, verbose=0)
    if isinstance(abp_approx, list):
        abp_approx = abp_approx[0]
    
    abp_refined = monitor_state['mdl2'].predict(abp_approx, verbose=0)
    if isinstance(abp_refined, list):
        abp_refined = abp_refined[0]
    
    # 反归一化
    meta = monitor_state['meta']
    abp_waveform = abp_refined.ravel() * (meta['max_abp'] - meta['min_abp']) + meta['min_abp']
    
    sbp = float(np.max(abp_waveform))
    dbp = float(np.min(abp_waveform))
    map_val = float(np.mean(abp_waveform))
    
    return sbp, dbp, map_val, ppg_resampled.tolist()


def get_status(sbp, dbp):
    """获取血压状态"""
    if sbp < 120 and dbp < 80:
        return "正常", "normal"
    elif sbp < 130 and dbp < 85:
        return "正常偏高", "elevated"
    elif sbp < 140 and dbp < 90:
        return "高血压前期", "high1"
    elif sbp < 160 and dbp < 100:
        return "1级高血压", "high2"
    else:
        return "2级高血压", "high3"


def measurement_loop():
    """测量主循环"""
    print("[调试] 测量循环已启动")
    while monitor_state['running']:
        try:
            # 读取数据
            print(f"[调试] 开始读取 {RAW_LENGTH} 个数据点...")
            ppg_raw = read_ppg_raw()
            print(f"[调试] 读取完成，数据范围: {ppg_raw.min():.0f} ~ {ppg_raw.max():.0f}")
            
            # 预测血压
            print("[调试] 开始预测...")
            sbp, dbp, map_val, ppg_wave = predict_bp(ppg_raw)
            print(f"[调试] 预测完成: SBP={sbp:.1f}, DBP={dbp:.1f}, MAP={map_val:.1f}")
            
            # 获取状态
            status_text, status_class = get_status(sbp, dbp)
            
            # 记录历史
            monitor_state['bp_history'].append({
                'sbp': sbp, 'dbp': dbp, 'map': map_val
            })
            if len(monitor_state['bp_history']) > 20:
                monitor_state['bp_history'].pop(0)
            
            # 推送到前端
            socketio.emit('bp_data', {
                'sbp': round(sbp, 1),
                'dbp': round(dbp, 1),
                'map': round(map_val, 1),
                'status': status_text,
                'status_class': status_class,
                'ppg': ppg_wave,
                'history': monitor_state['bp_history'],
                'timestamp': time.strftime('%H:%M:%S')
            })
            print("[调试] 数据已推送到前端")
            
        except Exception as e:
            print(f"[错误] 测量错误: {e}")
            import traceback
            traceback.print_exc()
            socketio.emit('error', {'message': str(e)})


@app.route('/')
def index():
    return render_template('index.html')


@socketio.on('start')
def handle_start(data):
    """开始测量"""
    if monitor_state['running']:
        return
    
    port = data.get('port', 'COM37')
    
    # 加载模型（首次）
    if monitor_state['mdl1'] is None:
        load_models()
    
    # 连接串口
    if not monitor_state['connected']:
        if not connect_serial(port):
            socketio.emit('error', {'message': f'无法连接 {port}'})
            return
    
    monitor_state['running'] = True
    print(f"[调试] 测量已启动，running={monitor_state['running']}")
    threading.Thread(target=measurement_loop, daemon=True).start()
    socketio.emit('status', {'message': '测量已开始', 'connected': True})


@socketio.on('stop')
def handle_stop():
    """停止测量"""
    monitor_state['running'] = False
    socketio.emit('status', {'message': '测量已停止', 'connected': False})


@socketio.on('connect')
def handle_connect():
    """客户端连接"""
    socketio.emit('status', {
        'message': '已连接服务器',
        'connected': monitor_state['connected'],
        'port': monitor_state['port']
    })


def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', default='COM37', help='串口号')
    parser.add_argument('--host', default='127.0.0.1', help='服务器地址')
    parser.add_argument('--webport', type=int, default=5000, help='Web 端口')
    args = parser.parse_args()
    
    monitor_state['port'] = args.port
    
    print("=" * 50)
    print("  PPG2ABP 实时血压监测 Web 仪表盘")
    print("=" * 50)
    print(f"\n  浏览器打开: http://{args.host}:{args.webport}")
    print(f"  串口: {args.port}")
    print(f"  按 Ctrl+C 退出\n")
    
    socketio.run(app, host=args.host, port=args.webport, debug=False)


if __name__ == '__main__':
    main()

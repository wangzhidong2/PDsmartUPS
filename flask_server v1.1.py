from flask import Flask, request, render_template, jsonify
import json
import os
import sys
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                           QLabel, QTextEdit, QPushButton, QHBoxLayout)
from PyQt5.QtCore import QThread, pyqtSignal, Qt
import threading

app = Flask(__name__)

# 日志存储文件
LOG_FILE = 'logs.json'

# Flask服务器线程
class FlaskThread(QThread):
    log_updated = pyqtSignal(str)
    
    def __init__(self):
        super().__init__()
        self.host = '0.0.0.0'
        self.port = 8080
        
    def run(self):
        print('Flask服务器启动中...')
        print(f'访问地址: http://{self.host}:{self.port}')
        print(f'API接口: http://{self.host}:{self.port}/api/logs')
        app.run(host=self.host, port=self.port, debug=False, use_reloader=False)

# 确保日志文件存在
if not os.path.exists(LOG_FILE):
    with open(LOG_FILE, 'w') as f:
        json.dump([], f)

# 读取日志
def read_logs():
    with open(LOG_FILE, 'r') as f:
        return json.load(f)

# 保存日志
def save_logs(logs):
    with open(LOG_FILE, 'w') as f:
        json.dump(logs, f, indent=2, ensure_ascii=False)

# 主窗口
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle('UPS日志监控服务器')
        self.setGeometry(100, 100, 800, 600)
        
        # 中央部件
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout(central_widget)
        
        # 服务器信息
        info_layout = QHBoxLayout()
        self.host_label = QLabel(f'主机: 0.0.0.0')
        self.port_label = QLabel(f'端口: 8080')
        self.url_label = QLabel(f'访问地址: http://0.0.0.0:8080')
        info_layout.addWidget(self.host_label)
        info_layout.addWidget(self.port_label)
        info_layout.addWidget(self.url_label)
        layout.addLayout(info_layout)
        
        # 日志显示
        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        self.log_text.setPlaceholderText('日志将显示在这里...')
        layout.addWidget(self.log_text)
        
        # 按钮
        btn_layout = QHBoxLayout()
        self.refresh_btn = QPushButton('刷新日志')
        self.refresh_btn.clicked.connect(self.refresh_logs)
        btn_layout.addWidget(self.refresh_btn)
        layout.addLayout(btn_layout)
        
        # 启动Flask服务器
        self.flask_thread = FlaskThread()
        self.flask_thread.start()
        
        # 初始刷新日志
        self.refresh_logs()
        
    def refresh_logs(self):
        logs = read_logs()
        logs.reverse()
        self.log_text.clear()
        for log in logs:
            self.log_text.append(f"[{log['time']}] [{log['level']}] {log['message']}")

# API接口，接收ESP32发送的日志
@app.route('/api/logs', methods=['POST'])
def receive_logs():
    try:
        data = request.get_json()
        if 'logs' in data:
            new_logs = data['logs']
            # 读取现有日志
            existing_logs = read_logs()
            # 添加新日志
            existing_logs.extend(new_logs)
            # 只保留最近100条日志
            if len(existing_logs) > 100:
                existing_logs = existing_logs[-100:]
            # 保存日志
            save_logs(existing_logs)
            return jsonify({'status': 'success', 'message': '日志接收成功'})
        else:
            return jsonify({'status': 'error', 'message': '无效的日志数据'}), 400
    except Exception as e:
        return jsonify({'status': 'error', 'message': str(e)}), 500

# 日志网页
@app.route('/')
def index():
    logs = read_logs()
    # 按时间倒序排列
    logs.reverse()
    return render_template('index.html', logs=logs)

# 日志数据API
@app.route('/api/logs/data')
def get_logs_data():
    logs = read_logs()
    return jsonify(logs)

# 主函数
if __name__ == '__main__':
    # 创建templates目录和index.html文件
    if not os.path.exists('templates'):
        os.makedirs('templates')
    
    # 创建HTML模板
    html_template = '''
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>UPS日志监控</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
            font-family: 'Microsoft YaHei', sans-serif;
        }
        body {
            background-color: #f5f5f5;
            color: #333;
            line-height: 1.6;
        }
        .container {
            max-width: 800px;
            margin: 20px auto;
            padding: 20px;
            background-color: white;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        h1 {
            color: #0078D4;
            margin-bottom: 20px;
            text-align: center;
        }
        .log-item {
            padding: 10px;
            border-bottom: 1px solid #eee;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .log-item:last-child {
            border-bottom: none;
        }
        .log-time {
            font-weight: bold;
            color: #666;
        }
        .log-level {
            padding: 2px 8px;
            border-radius: 12px;
            font-size: 12px;
            font-weight: bold;
        }
        .log-level.INFO {
            background-color: #e6f7ff;
            color: #1890ff;
        }
        .log-level.ERROR {
            background-color: #fff2f0;
            color: #ff4d4f;
        }
        .log-message {
            flex: 1;
            margin: 0 10px;
        }
        .empty-log {
            text-align: center;
            color: #999;
            padding: 40px;
        }
        .refresh-btn {
            display: block;
            margin: 20px auto;
            padding: 8px 16px;
            background-color: #0078D4;
            color: white;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 14px;
        }
        .refresh-btn:hover {
            background-color: #005a9e;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>UPS系统日志</h1>
        <button class="refresh-btn" onclick="location.reload()">刷新日志</button>
        <div id="log-container">
            {% if logs %}
                {% for log in logs %}
                <div class="log-item">
                    <span class="log-time">{{ log.time }}</span>
                    <span class="log-level {{ log.level }}">{{ log.level }}</span>
                    <span class="log-message">{{ log.message }}</span>
                </div>
                {% endfor %}
            {% else %}
                <div class="empty-log">暂无日志数据</div>
            {% endif %}
        </div>
    </div>
</body>
</html>
    '''
    
    with open('templates/index.html', 'w', encoding='utf-8') as f:
        f.write(html_template)
    
    # 创建Qt应用
    qt_app = QApplication(sys.argv)
    
    # 创建主窗口
    window = MainWindow()
    window.show()
    
    # 运行Qt事件循环
    sys.exit(qt_app.exec_())

from flask import Flask, request, render_template, jsonify
import json
import os
import sys
import threading

# 尝试导入PyQt5，如果失败则只运行Flask部分
HAS_PYQT5 = False
try:
    from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                               QLabel, QTextEdit, QPushButton, QHBoxLayout, QFrame, 
                               QToolBar, QAction, QSizePolicy, QSplitter)
    from PyQt5.QtCore import QThread, pyqtSignal, Qt, QSize
    from PyQt5.QtGui import QIcon, QFont, QColor
    HAS_PYQT5 = True
except ImportError:
    print("PyQt5 not available, running in Flask-only mode")

app = Flask(__name__)

# 日志存储文件
LOG_FILE = 'logs.json'

# Flask服务器线程
class FlaskThread(threading.Thread):
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
if HAS_PYQT5:
    class MainWindow(QMainWindow):
        def __init__(self):
            super().__init__()
            self.setWindowTitle('UPS日志监控服务器')
            self.setGeometry(100, 100, 1000, 700)
            
            # 设置窗口风格
            self.setStyleSheet('''
                QMainWindow {
                    background-color: #f3f4f6;
                }
                QFrame {
                    background-color: white;
                    border-radius: 8px;
                    border: 1px solid #e5e7eb;
                }
                QLabel {
                    color: #1f2937;
                    font-family: 'Segoe UI', sans-serif;
                }
                QPushButton {
                    background-color: #0078d4;
                    color: white;
                    border: none;
                    border-radius: 4px;
                    padding: 6px 12px;
                    font-family: 'Segoe UI', sans-serif;
                    font-size: 14px;
                }
                QPushButton:hover {
                    background-color: #005a9e;
                }
                QPushButton:pressed {
                    background-color: #004578;
                }
                QTextEdit {
                    background-color: white;
                    border: 1px solid #e5e7eb;
                    border-radius: 4px;
                    font-family: 'Consolas', monospace;
                    font-size: 13px;
                }
                QToolBar {
                    background-color: #f9fafb;
                    border: none;
                    spacing: 8px;
                }
                QAction {
                    padding: 8px;
                }
                QAction:hover {
                    background-color: #e5e7eb;
                    border-radius: 4px;
                }
            ''')
            
            # 中央部件
            central_widget = QWidget()
            self.setCentralWidget(central_widget)
            main_layout = QHBoxLayout(central_widget)
            main_layout.setContentsMargins(16, 16, 16, 16)
            main_layout.setSpacing(16)
            
            # 侧边栏
            self.sidebar = QFrame()
            self.sidebar.setMinimumWidth(200)
            self.sidebar.setMaximumWidth(250)
            sidebar_layout = QVBoxLayout(self.sidebar)
            sidebar_layout.setContentsMargins(16, 16, 16, 16)
            sidebar_layout.setSpacing(12)
            
            # 侧边栏标题
            sidebar_title = QLabel('UPS监控')
            sidebar_title.setFont(QFont('Segoe UI', 16, QFont.Bold))
            sidebar_title.setStyleSheet('color: #1f2937;')
            sidebar_layout.addWidget(sidebar_title)
            
            # 服务器信息
            info_group = QFrame()
            info_layout = QVBoxLayout(info_group)
            info_layout.setContentsMargins(12, 12, 12, 12)
            
            server_label = QLabel('服务器信息')
            server_label.setFont(QFont('Segoe UI', 12, QFont.Medium))
            server_label.setStyleSheet('color: #4b5563; margin-bottom: 8px;')
            info_layout.addWidget(server_label)
            
            self.host_label = QLabel(f'主机: 0.0.0.0')
            self.host_label.setFont(QFont('Segoe UI', 11))
            info_layout.addWidget(self.host_label)
            
            self.port_label = QLabel(f'端口: 8080')
            self.port_label.setFont(QFont('Segoe UI', 11))
            info_layout.addWidget(self.port_label)
            
            self.url_label = QLabel(f'访问地址: http://0.0.0.0:8080')
            self.url_label.setFont(QFont('Segoe UI', 11))
            self.url_label.setWordWrap(True)
            info_layout.addWidget(self.url_label)
            
            sidebar_layout.addWidget(info_group)
            
            # 操作按钮
            action_group = QFrame()
            action_layout = QVBoxLayout(action_group)
            action_layout.setContentsMargins(12, 12, 12, 12)
            action_layout.setSpacing(8)
            
            action_label = QLabel('操作')
            action_label.setFont(QFont('Segoe UI', 12, QFont.Medium))
            action_label.setStyleSheet('color: #4b5563; margin-bottom: 8px;')
            action_layout.addWidget(action_label)
            
            self.refresh_btn = QPushButton('刷新日志')
            self.refresh_btn.clicked.connect(self.refresh_logs)
            action_layout.addWidget(self.refresh_btn)
            
            sidebar_layout.addWidget(action_group)
            
            # 填充空间
            sidebar_layout.addStretch()
            
            # 主内容区域
            self.main_content = QFrame()
            content_layout = QVBoxLayout(self.main_content)
            content_layout.setContentsMargins(16, 16, 16, 16)
            content_layout.setSpacing(12)
            
            # 标题栏
            title_layout = QHBoxLayout()
            title = QLabel('系统日志')
            title.setFont(QFont('Segoe UI', 18, QFont.Bold))
            title.setStyleSheet('color: #1f2937;')
            title_layout.addWidget(title)
            title_layout.addStretch()
            content_layout.addLayout(title_layout)
            
            # 日志显示
            self.log_text = QTextEdit()
            self.log_text.setReadOnly(True)
            self.log_text.setPlaceholderText('日志将显示在这里...')
            self.log_text.setStyleSheet('''
                QTextEdit {
                    background-color: white;
                    border: 1px solid #e5e7eb;
                    border-radius: 8px;
                    font-family: 'Consolas', monospace;
                    font-size: 13px;
                    padding: 12px;
                }
            ''')
            content_layout.addWidget(self.log_text)
            
            # 底部信息
            status_layout = QHBoxLayout()
            self.status_label = QLabel('就绪')
            self.status_label.setFont(QFont('Segoe UI', 11))
            self.status_label.setStyleSheet('color: #6b7280;')
            status_layout.addWidget(self.status_label)
            status_layout.addStretch()
            content_layout.addLayout(status_layout)
            
            # 创建分割器
            splitter = QSplitter(Qt.Horizontal)
            splitter.addWidget(self.sidebar)
            splitter.addWidget(self.main_content)
            splitter.setSizes([200, 800])
            splitter.setHandleWidth(4)
            splitter.setStyleSheet('''
                QSplitter::handle {
                    background-color: #e5e7eb;
                    border-radius: 2px;
                }
                QSplitter::handle:hover {
                    background-color: #d1d5db;
                }
            ''')
            
            main_layout.addWidget(splitter)
            
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
                if log['level'] == 'ERROR':
                    self.log_text.append(f"<font color='#dc2626'>[{log['time']}] [{log['level']}] {log['message']}</font>")
                elif log['level'] == 'INFO':
                    self.log_text.append(f"<font color='#1e40af'>[{log['time']}] [{log['level']}] {log['message']}</font>")
                else:
                    self.log_text.append(f"[{log['time']}] [{log['level']}] {log['message']}")
            self.status_label.setText(f'显示 {len(logs)} 条日志')

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
        :root {
            --primary-color: #0078D4;
            --secondary-color: #f3f4f6;
            --text-color: #1f2937;
            --border-color: #e5e7eb;
            --success-color: #10b981;
            --warning-color: #f59e0b;
            --error-color: #ef4444;
            --info-color: #3b82f6;
            --card-bg: #ffffff;
            --sidebar-bg: #f9fafb;
        }
        
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        }
        
        body {
            background-color: var(--secondary-color);
            color: var(--text-color);
            line-height: 1.6;
        }
        
        .container {
            max-width: 1200px;
            margin: 0 auto;
            padding: 20px;
        }
        
        .app-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 30px;
        }
        
        .app-header h1 {
            color: var(--text-color);
            font-size: 24px;
            font-weight: 600;
        }
        
        .content-layout {
            display: flex;
            gap: 20px;
        }
        
        .sidebar {
            width: 280px;
            background-color: var(--sidebar-bg);
            border-radius: 8px;
            padding: 24px;
            box-shadow: 0 1px 3px rgba(0,0,0,0.1);
        }
        
        .sidebar-section {
            margin-bottom: 24px;
        }
        
        .sidebar-section h3 {
            font-size: 14px;
            font-weight: 600;
            color: #4b5563;
            margin-bottom: 12px;
        }
        
        .sidebar-info {
            background-color: var(--card-bg);
            border-radius: 6px;
            padding: 16px;
            border: 1px solid var(--border-color);
        }
        
        .info-item {
            margin-bottom: 8px;
            font-size: 14px;
        }
        
        .info-label {
            color: #6b7280;
            font-weight: 500;
        }
        
        .info-value {
            color: var(--text-color);
        }
        
        .action-buttons {
            display: flex;
            flex-direction: column;
            gap: 10px;
        }
        
        .btn {
            background-color: var(--primary-color);
            color: white;
            border: none;
            border-radius: 6px;
            padding: 10px 16px;
            font-size: 14px;
            font-weight: 500;
            cursor: pointer;
            transition: background-color 0.2s;
        }
        
        .btn:hover {
            background-color: #005a9e;
        }
        
        .btn:active {
            background-color: #004578;
        }
        
        .main-content {
            flex: 1;
        }
        
        .card {
            background-color: var(--card-bg);
            border-radius: 8px;
            padding: 24px;
            box-shadow: 0 1px 3px rgba(0,0,0,0.1);
        }
        
        .card-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 20px;
        }
        
        .card-title {
            font-size: 18px;
            font-weight: 600;
            color: var(--text-color);
        }
        
        .log-container {
            margin-top: 20px;
        }
        
        .log-item {
            padding: 16px;
            border-bottom: 1px solid var(--border-color);
            display: flex;
            align-items: center;
            gap: 16px;
        }
        
        .log-item:last-child {
            border-bottom: none;
        }
        
        .log-time {
            font-size: 14px;
            font-weight: 500;
            color: #6b7280;
            min-width: 150px;
        }
        
        .log-level {
            font-size: 12px;
            font-weight: 600;
            padding: 4px 12px;
            border-radius: 12px;
            min-width: 70px;
            text-align: center;
        }
        
        .log-level.INFO {
            background-color: #e0f2fe;
            color: #0284c7;
        }
        
        .log-level.ERROR {
            background-color: #fee2e2;
            color: #dc2626;
        }
        
        .log-level.WARNING {
            background-color: #fef3c7;
            color: #d97706;
        }
        
        .log-message {
            flex: 1;
            font-size: 14px;
            color: var(--text-color);
        }
        
        .empty-state {
            text-align: center;
            padding: 60px 20px;
            color: #6b7280;
        }
        
        .empty-state-icon {
            font-size: 48px;
            margin-bottom: 16px;
            opacity: 0.5;
        }
        
        @media (max-width: 768px) {
            .content-layout {
                flex-direction: column;
            }
            
            .sidebar {
                width: 100%;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="app-header">
            <h1>UPS系统日志监控</h1>
        </div>
        
        <div class="content-layout">
            <div class="sidebar">
                <div class="sidebar-section">
                    <h3>服务器信息</h3>
                    <div class="sidebar-info">
                        <div class="info-item">
                            <span class="info-label">主机:</span>
                            <span class="info-value">0.0.0.0</span>
                        </div>
                        <div class="info-item">
                            <span class="info-label">端口:</span>
                            <span class="info-value">8080</span>
                        </div>
                        <div class="info-item">
                            <span class="info-label">访问地址:</span>
                            <span class="info-value">http://0.0.0.0:8080</span>
                        </div>
                    </div>
                </div>
                
                <div class="sidebar-section">
                    <h3>操作</h3>
                    <div class="action-buttons">
                        <button class="btn" onclick="location.reload()">刷新日志</button>
                    </div>
                </div>
            </div>
            
            <div class="main-content">
                <div class="card">
                    <div class="card-header">
                        <h2 class="card-title">系统日志</h2>
                    </div>
                    
                    <div class="log-container">
                        {% if logs %}
                            {% for log in logs %}
                            <div class="log-item">
                                <span class="log-time">{{ log.time }}</span>
                                <span class="log-level {{ log.level }}">{{ log.level }}</span>
                                <span class="log-message">{{ log.message }}</span>
                            </div>
                            {% endfor %}
                        {% else %}
                            <div class="empty-state">
                                <div class="empty-state-icon">📋</div>
                                <p>暂无日志数据</p>
                            </div>
                        {% endif %}
                    </div>
                </div>
            </div>
        </div>
    </div>
</body>
</html>
    '''
    
    with open('templates/index.html', 'w', encoding='utf-8') as f:
        f.write(html_template)
    
    if HAS_PYQT5:
        # 创建Qt应用
        qt_app = QApplication(sys.argv)
        
        # 创建主窗口
        window = MainWindow()
        window.show()
        
        # 运行Qt事件循环
        sys.exit(qt_app.exec_())
    else:
        # 直接运行Flask服务器
        print('Flask服务器启动中...')
        print('访问地址: http://0.0.0.0:8080')
        print('API接口: http://0.0.0.0:8080/api/logs')
        app.run(host='0.0.0.0', port=8080, debug=False, use_reloader=False)

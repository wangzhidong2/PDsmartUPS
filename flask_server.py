from flask import Flask, request, render_template, jsonify
import json
import os

app = Flask(__name__)

# 日志存储文件
LOG_FILE = 'logs.json'

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

# API接口，接收ESP8266发送的日志
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
    
    print('Flask服务器启动中...')
    print('访问地址: http://localhost:5000')
    print('API接口: http://localhost:5000/api/logs')
    app.run(host='0.0.0.0', port=5000, debug=True)

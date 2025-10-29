# app.py - Improved Version
from flask import Flask, request, jsonify, render_template, g
import sqlite3
import time
import threading
import requests
import pandas as pd
from datetime import datetime, timedelta
import json

DB_PATH = 'flood_logs.db'
BOT_TOKEN = "8478531200:AAHLqRv3i92sztYXL-KLkTE-Jt8Q3KkpkQQ"
CHAT_ID = '1964720324'
ALERT_THRESHOLD = 85
CHECK_TELEGRAM_POLL_SEC = 5
TELEGRAM_API_URL = f"https://api.telegram.org/bot{BOT_TOKEN}"

app = Flask(__name__, template_folder='templates')

# State untuk mencegah spam alert
last_alert_time = None
ALERT_COOLDOWN_MINUTES = 10

def get_db():
    db = getattr(g, '_database', None)
    if db is None:
        db = g._database = sqlite3.connect(DB_PATH, check_same_thread=False)
        db.row_factory = sqlite3.Row
    return db

def init_db():
    db = get_db()
    db.execute('''
    CREATE TABLE IF NOT EXISTS logs (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        device_id TEXT,
        ts INTEGER,
        ts_iso TEXT,
        ultrasonic_cm REAL,
        level_percent INTEGER,
        water_level_percent INTEGER,
        final_level_percent INTEGER,
        sensor_status TEXT
    )
    ''')
    
    # Tambah index untuk performa query
    db.execute('CREATE INDEX IF NOT EXISTS idx_ts ON logs(ts)')
    db.execute('CREATE INDEX IF NOT EXISTS idx_device ON logs(device_id)')
    db.commit()

@app.teardown_appcontext
def close_connection(exception):
    db = getattr(g, '_database', None)
    if db is not None:
        db.close()

def insert_log(device_id, ts, ultrasonic_cm, level_percent, water_level_percent=None, final_level_percent=None, sensor_status=None):
    db = get_db()
    ts_iso = datetime.utcfromtimestamp(ts).isoformat()
    db.execute('''
        INSERT INTO logs (device_id, ts, ts_iso, ultrasonic_cm, level_percent, water_level_percent, final_level_percent, sensor_status) 
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    ''', (device_id, ts, ts_iso, ultrasonic_cm, level_percent, water_level_percent, final_level_percent, sensor_status))
    db.commit()

def query_logs(limit=1000, device_id=None, hours=None):
    db = get_db()
    query = 'SELECT * FROM logs WHERE 1=1'
    params = []
    
    if device_id:
        query += ' AND device_id = ?'
        params.append(device_id)
    
    if hours:
        cutoff_ts = int((datetime.now() - timedelta(hours=hours)).timestamp())
        query += ' AND ts >= ?'
        params.append(cutoff_ts)
    
    query += ' ORDER BY ts DESC LIMIT ?'
    params.append(limit)
    
    cur = db.execute(query, params)
    rows = cur.fetchall()
    return [dict(r) for r in rows]

def send_telegram_message(text, parse_mode='Markdown'):
    url = f"{TELEGRAM_API_URL}/sendMessage"
    payload = {
        'chat_id': CHAT_ID, 
        'text': text,
        'parse_mode': parse_mode
    }
    try:
        r = requests.post(url, json=payload, timeout=10)
        if not r.ok:
            print(f"Telegram API error: {r.status_code} - {r.text}")
        return r.ok
    except Exception as e:
        print("Telegram send error:", e)
        return False

def check_alert_conditions(level_percent, device_id, ts):
    global last_alert_time
    
    # Cooldown untuk mencegah spam
    if last_alert_time and (time.time() - last_alert_time) < ALERT_COOLDOWN_MINUTES * 60:
        return False
    
    if level_percent >= ALERT_THRESHOLD:
        last_alert_time = time.time()
        
        # Buat pesan alert yang lebih informatif
        alert_level = "🚨 KRITIS" if level_percent >= 95 else "⚠️ WASPADA"
        local_time = datetime.fromtimestamp(ts).strftime('%Y-%m-%d %H:%M:%S')
        
        text = f"""
{alert_level} - DETEKSI BANJIR

📊 **Level Air**: {level_percent}%
📱 **Device**: {device_id}
🕐 **Waktu**: {local_time}
📍 **Status**: {'BAHAYA TINGGI' if level_percent >= 95 else 'WASPADA'}

Tindakan segera diperlukan!
"""
        return send_telegram_message(text)
    return False

@app.route('/')
def index():
    return render_template('dashboard.html')


@app.route('/data', methods=['POST'])
def receive_data():
    try:
        j = request.get_json(force=True)
        print(f"📨 Received JSON: {j}")  # DEBUG
    except Exception as e:
        print(f"JSON parse error: {e}")
        return jsonify({'ok': False, 'error': 'invalid json'}), 400

    # Extract data - PERBAIKI: gunakan final_level_percent sebagai level_percent
    device_id = j.get('device_id', 'unknown')
    timestamp_ms = j.get('timestamp_ms', int(time.time() * 1000))
    ts = int(timestamp_ms / 1000)
    ultrasonic_cm = float(j.get('ultrasonic_cm', -1))
    
    # PERBAIKAN: Gunakan final_level_percent sebagai level_percent utama
    level_percent = j.get('final_level_percent', j.get('level_percent', -1))
    level_percent = int(level_percent)
    
    water_level_percent = j.get('water_level_percent')
    final_level_percent = j.get('final_level_percent')
    sensor_status = j.get('sensor_status')
    
    # Validasi data
    if level_percent < 0 or level_percent > 100:
        print(f"❌ Invalid level_percent: {level_percent}")
        return jsonify({'ok': False, 'error': 'invalid level_percent'}), 400

    # Simpan ke database
    insert_log(device_id, ts, ultrasonic_cm, level_percent, water_level_percent, final_level_percent, sensor_status)
    
    print(f"✅ Data saved: {device_id} - {level_percent}% - {ultrasonic_cm}cm")
    
    # Check alert conditions
    check_alert_conditions(level_percent, device_id, ts)
    
    return jsonify({'ok': True, 'received': {
        'device_id': device_id,
        'level_percent': level_percent,
        'ultrasonic_cm': ultrasonic_cm
    }})

@app.route('/api/logs')
def api_logs():
    limit = int(request.args.get('limit', 1000))
    device_id = request.args.get('device_id')
    hours = request.args.get('hours', type=int)
    
    rows = query_logs(limit=limit, device_id=device_id, hours=hours)
    return jsonify(rows)

@app.route('/api/stats')
def api_stats():
    device_id = request.args.get('device_id')
    hours = request.args.get('hours', 24, type=int)  # Default 24 jam
    
    rows = query_logs(limit=10000, device_id=device_id, hours=hours)
    if not rows:
        return jsonify({'ok': True, 'count': 0, 'data': {}})
    
    df = pd.DataFrame(rows)
    stats = {
        'count': len(df),
        'current': int(df.iloc[0]['level_percent']) if not df.empty else 0,
        'mean': float(df['level_percent'].mean()),
        'max': int(df['level_percent'].max()),
        'min': int(df['level_percent'].min()),
        'std': float(df['level_percent'].std())
    }
    
    return jsonify({'ok': True, 'stats': stats})

@app.route('/api/alert_test')
def api_alert_test():
    """Endpoint untuk testing alert"""
    level = request.args.get('level', ALERT_THRESHOLD, type=int)
    result = check_alert_conditions(level, 'test_device', time.time())
    return jsonify({'ok': True, 'alert_sent': result})

# Telegram bot handler
last_update_id = None

def poll_telegram():
    global last_update_id
    print("Telegram poller started")
    while True:
        try:
            url = f"{TELEGRAM_API_URL}/getUpdates"
            params = {'timeout': 10}
            if last_update_id:
                params['offset'] = last_update_id + 1
                
            r = requests.get(url, params=params, timeout=30).json()
            if not r.get('ok'):
                time.sleep(2)
                continue
                
            for upd in r.get('result', []):
                last_update_id = upd['update_id']
                if 'message' in upd:
                    chat = upd['message']['chat']
                    chat_id = chat['id']
                    text = upd['message'].get('text', '').strip()
                    
                    # hanya respon bila dari CHAT_ID
                    if str(chat_id) != str(CHAT_ID):
                        continue
                        
                    if text.startswith('/status'):
                        rows = query_logs(limit=1)
                        if rows:
                            row = rows[0]  # Latest record
                            status_emoji = "🔴" if row['level_percent'] >= ALERT_THRESHOLD else "🟡" if row['level_percent'] >= 60 else "🟢"
                            msg = f"""
{status_emoji} **Status Terkini**

📱 Device: {row['device_id']}
💧 Level Air: {row['level_percent']}%
📏 Ultrasonic: {row['ultrasonic_cm']} cm
🕐 Waktu: {row['ts_iso']} UTC
🔧 Sensor: {row.get('sensor_status', 'N/A')}

{'🚨 **ALERT ACTIVE**' if row['level_percent'] >= ALERT_THRESHOLD else '✅ Normal'}
"""
                        else:
                            msg = "📭 Belum ada data."
                        send_telegram_message(msg)
                        
                    elif text.startswith('/stats'):
                        hours = 24
                        if ' ' in text:
                            try:
                                hours = int(text.split()[1])
                            except:
                                pass
                                
                        rj = requests.get(f'http://127.0.0.1:5000/api/stats?hours={hours}').json()
                        if rj['ok']:
                            s = rj['stats']
                            msg = f"""
📈 **Statistik ({hours} jam)**

📊 Sample: {s['count']}
🌊 Current: {s['current']}%
📊 Average: {s['mean']:.1f}%
📈 Maximum: {s['max']}%
📉 Minimum: {s['min']}%
"""
                        else:
                            msg = "❌ Gagal mengambil statistik"
                        send_telegram_message(msg)
                        
                    elif text.startswith('/help'):
                        msg = """
🤖 **Flood Detection Bot**

/status - Status terkini
/stats [hours] - Statistik (default 24 jam)
/help - Bantuan ini

🚨 Alert otomatis aktif di atas 85%
"""
                        send_telegram_message(msg)
                        
        except Exception as e:
            print("Poll error:", e)
            time.sleep(5)

if __name__ == '__main__':
    with app.app_context():
        init_db()
    
    # Jalankan polling telegram di thread terpisah
    t = threading.Thread(target=poll_telegram, daemon=True)
    t.start()
    
    print(f"Flood Detection Server started on http://0.0.0.0:5000")
    print(f"Alert threshold: {ALERT_THRESHOLD}%")
    print(f"Telegram Chat ID: {CHAT_ID}")
    
    app.run(host='0.0.0.0', port=5000, debug=False)
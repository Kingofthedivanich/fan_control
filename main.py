from flask import Flask, render_template_string, request, jsonify
import requests

app = Flask(__name__)

# Глобально храним IP ESP32 (устанавливается через /receive_ip)
esp32_ip = None

# HTML-шаблон страницы управления
HTML_TEMPLATE = """
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <title>Управление вентилятором ESP32</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; padding: 40px; }
        input[type=range] { width: 60%%; }
        #value { font-size: 24px; margin-top: 10px; }
        .ip { margin-bottom: 20px; color: #555; }
    </style>
</head>
<body>
    <h1>Контроль скорости вентилятора</h1>
    <div class="ip">
        {% if esp32_ip %}
            <strong>IP ESP32:</strong> {{ esp32_ip }}
        {% else %}
            <strong>ESP32 не подключён</strong>
        {% endif %}
    </div>

    <input type="range" min="0" max="255" value="0" id="speedSlider">
    <div id="value">Скорость: 0</div>

    <script>
        const slider = document.getElementById("speedSlider");
        const valueDisplay = document.getElementById("value");

        slider.oninput = function() {
            let speed = this.value;
            valueDisplay.textContent = "Скорость: " + speed;

            fetch('/update?speed=' + speed)
                .catch(err => console.error('Ошибка запроса:', err));
        };
    </script>
</body>
</html>
"""

@app.route("/")
def index():
    return render_template_string(HTML_TEMPLATE, esp32_ip=esp32_ip)

@app.route("/update")
def update_speed():
    global esp32_ip
    speed = request.args.get("speed", default=None)

    if not esp32_ip:
        return "ESP32 не подключён", 400

    if speed is not None and speed.isdigit():
        speed_val = int(speed)
        if 0 <= speed_val <= 255:
            try:
                url = f"{esp32_ip}/setSpeed?speed={speed_val}"
                print(f"[DEBUG] Запрос к ESP32: {url}")
                response = requests.get(url, timeout=2)
                return response.text, response.status_code
            except Exception as e:
                print(f"[ERROR] Ошибка при подключении к ESP32: {e}")
                return f"Ошибка подключения к ESP32: {e}", 500
    return "Неверное значение скорости", 400

@app.route("/receive_ip", methods=["POST"])
def receive_ip():
    global esp32_ip
    data = request.get_json()
    ip = data.get("ip")
    if ip:
        esp32_ip = f"http://{ip}"
        print(f"[INFO] Получен IP от ESP32: {esp32_ip}")
        return jsonify({"status": "success", "ip": ip}), 200
    else:
        return jsonify({"status": "error", "message": "IP не передан"}), 400

if __name__ == "__main__":
    # Самое важное — host="0.0.0.0", чтобы ESP32 мог подключиться
    app.run(host="0.0.0.0", port=5000, debug=True)

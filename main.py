import datetime
from http.server import HTTPServer, SimpleHTTPRequestHandler
import json
import sqlite3
from urllib.parse import urlparse, parse_qs


DATABASE_NAME = "MonitorPflanzendaten.db"


def initDatabase():

    con = sqlite3.connect(DATABASE_NAME)
    cur = con.cursor()

    cur.execute("""
        CREATE TABLE IF NOT EXISTS Data_plants(
            Timestamp TEXT,
            MAC TEXT,
            Temp REAL,
            Moisture REAL,
            Conductivity REAL,
            Illuminance REAL,
            JSON TEXT
        )
    """)

    con.commit()
    con.close()

    print("Datenbank bereit.")

def saveData(data):

    con = sqlite3.connect(DATABASE_NAME)
    cur = con.cursor()

    timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    cur.execute("""
        INSERT INTO Data_plants(
            Timestamp,
            MAC,
            Temp,
            Moisture,
            Conductivity,
            Illuminance,
            JSON
        )
        VALUES (?, ?, ?, ?, ?, ?, ?)
    """, (
        timestamp,
        data["mac"],
        data["temp"],
        data["moisture"],
        data["conductivity"],
        data["light"],
        json.dumps(data)
    ))

    con.commit()
    con.close()

    print("Daten gespeichert.")


def getLatestSensorData(mac):

    con = sqlite3.connect(DATABASE_NAME)
    cur = con.cursor()

    row = cur.execute("""
        SELECT Temp, Moisture, Conductivity, Illuminance, Timestamp
        FROM Data_plants
        WHERE MAC = ?
        ORDER BY Timestamp DESC
        LIMIT 1
    """, (mac,)).fetchone()

    con.close()

    if row:
        return {
            "temp": row[0],
            "moisture": row[1],
            "conductivity": row[2],
            "light": row[3],
            "timestamp": row[4]
        }

    return None


def getSensorHistory(mac, limit=50):

    con = sqlite3.connect(DATABASE_NAME)
    cur = con.cursor()

    rows = cur.execute("""
        SELECT Temp, Moisture, Conductivity, Illuminance, Timestamp
        FROM Data_plants
        WHERE MAC = ?
        ORDER BY Timestamp ASC
        LIMIT ?
    """, (mac, limit)).fetchall()

    con.close()

    history = []

    for row in rows:

        history.append({
            "temp": row[0],
            "moisture": row[1],
            "conductivity": row[2],
            "light": row[3],
            "timestamp": row[4]
        })

    return history


def get_sensor_adress():

    con = sqlite3.connect(DATABASE_NAME)
    cur = con.cursor()

    rows = cur.execute("""
        SELECT mac, name
        FROM sensors s1
        WHERE NOT EXISTS(
        SELECT mac 
        FROM sensors_esp s2
        Where s1.mac=s2.mac);
    """).fetchall()

    con.close()

    new_sensors = []

    for row in rows: 
        
        new_sensors.append({
            "mac":row[0],
            "name":row[1]
        })

    return new_sensors


def updatedb():
    con = sqlite3.connect(DATABASE_NAME)
    cur = con.cursor()

    cur.execute("""
        INSERT INTO sensors_esp (mac, name)
        SELECT s1.mac, s1.name
        FROM sensors s1
        WHERE NOT EXISTS (
            SELECT 1
            FROM sensors_esp s2
            WHERE s1.mac = s2.mac
        )
    """)
    con.commit()
    con.close()



class SimpleHandler(SimpleHTTPRequestHandler):


    def do_POST(self):

        content_length = int(self.headers["Content-Length"])
        raw_data = self.rfile.read(content_length)

        print("\nRaw Data:", raw_data)

        try:

            data = json.loads(raw_data)

            print("JSON empfangen:")
            print(data)

            saveData(data)
            new_sensors= json.dumps(get_sensor_adress())
            self.send_response(200,new_sensors)
            self.end_headers()

            self.wfile.write(b"JSON OK")
            updatedb()

        except json.JSONDecodeError:

            self.send_response(400)
            self.end_headers()

            self.wfile.write(b"Invalid JSON")

        except Exception as e:

            print(e)

            self.send_response(500)
            self.end_headers()

            self.wfile.write(b"Server Error")


    def do_GET(self):

        parsed_path = urlparse(self.path)
        query = parse_qs(parsed_path.query)

        mac = query.get("mac", [None])[0]

        if parsed_path.path == "/sensor":

            if not mac:

                self.send_response(400)
                self.end_headers()

                self.wfile.write(b"Missing MAC")

                return

            data = getLatestSensorData(mac)

            if data:

                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()

                self.wfile.write(json.dumps(data).encode())

            else:

                self.send_response(404)
                self.end_headers()

                self.wfile.write(b"No data found")

            return

        if parsed_path.path == "/history":

            if not mac:

                self.send_response(400)
                self.end_headers()

                self.wfile.write(b"Missing MAC")

                return

            history = getSensorHistory(mac)

            

            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()

            self.wfile.write(json.dumps(history).encode())

            return

        self.send_response(404)
        self.end_headers()

        self.wfile.write(b"Endpoint not found")


def startServer():

    server = HTTPServer(("0.0.0.0", 8080), SimpleHandler)

    print("Server läuft auf Port 8080...")

    server.serve_forever()


if __name__ == "__main__":

    initDatabase()

    startServer()
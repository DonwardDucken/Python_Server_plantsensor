import datetime
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
import json
import sqlite3
from urllib.parse import urlparse, parse_qs

DATABASE_NAME = "MonitorPflanzendaten.db"


def get_connection():
    return sqlite3.connect(DATABASE_NAME)



def initDatabase():
    print("Datenbank wird initialisiert...")
    con = get_connection()
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

    cur.execute("""
       CREATE TABLE IF NOT EXISTS Data_sensor(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    MAC TEXT,
    Pflanzenname TEXT,
    plant_name TEXT,
    species_id TEXT,
    room TEXT,
    image_uri TEXT,
    last_watered TEXT,
    care_hints TEXT
)
    """)

    cur.execute("""
        CREATE TABLE IF NOT EXISTS sensors(
            mac TEXT,
            name TEXT
        )
    """)

    cur.execute("""
        CREATE TABLE IF NOT EXISTS sensors_esp(
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT,
            mac TEXT UNIQUE
        )
    """)

    con.commit()
    con.close()
    print("Datenbank bereit.")


def saveData(data):
    con = get_connection()
    cur = con.cursor()

    timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    cur.execute("""
        INSERT INTO Data_plants(
            Timestamp, MAC, Temp, Moisture, Conductivity, Illuminance, JSON
        )
        VALUES (?, ?, ?, ?, ?, ?, ?)
    """, (
        timestamp,
        data.get("mac"),
        data.get("temp"),
        data.get("moisture"),
        data.get("conductivity"),
        data.get("light"),
        json.dumps(data)
    ))

    con.commit()
    con.close()


def getLatestSensorData(mac):
    con = get_connection()
    cur = con.cursor()

    row = cur.execute("""
        SELECT Temp, Moisture, Conductivity, Illuminance, Timestamp
        FROM Data_plants
        WHERE MAC = ?
        ORDER BY Timestamp DESC
        LIMIT 1
    """, (mac,)).fetchone()

    con.close()

    if not row:
        return None

    return {
        "temp": row[0],
        "moisture": row[1],
        "conductivity": row[2],
        "light": row[3],
        "timestamp": row[4]
    }


def getSensorHistory(mac, limit=50):
    con = get_connection()
    cur = con.cursor()

    rows = cur.execute("""
        SELECT Temp, Moisture, Conductivity, Illuminance, Timestamp
        FROM Data_plants
        WHERE MAC = ?
        ORDER BY Timestamp DESC
        LIMIT ?
    """, (mac, limit)).fetchall()

    con.close()

    return [
        {
            "temp": row[0],
            "moisture": row[1],
            "conductivity": row[2],
            "light": row[3],
            "timestamp": row[4]
        }
        for row in rows
    ]


def getPlants():
    con = sqlite3.connect(DATABASE_NAME)
    cur = con.cursor()

    rows = cur.execute("""
        SELECT id, plant_name, species_id, room,
               MAC, image_uri, last_watered, care_hints
        FROM Data_sensor
    """).fetchall()

    con.close()

    plants = []

    for row in rows:
        plants.append({
            "id": row[0],
            "plant_name": row[1],
            "species_id": row[2],
            "room": row[3],
            "MAC": row[4],
            "image_uri": row[5],
            "last_watered": row[6],
            "care_hints": row[7]
        })

    return plants


def addPlant(data):
    con = sqlite3.connect(DATABASE_NAME)
    cur = con.cursor()

    cur.execute("""
        INSERT INTO Data_sensor(
            plant_name,
            species_id,
            room,
            MAC,
            image_uri,
            last_watered,
            care_hints
        )
        VALUES (?, ?, ?, ?, ?, ?, ?)
    """, (
        data.get("plant_name"),
        data.get("species_id"),
        data.get("room"),
        data.get("MAC"),
        data.get("image_uri"),
        data.get("last_watered"),
        data.get("care_hints", "")
    ))

    con.commit()
    con.close()

def updatePlant(data):
    con = sqlite3.connect(DATABASE_NAME)
    cur = con.cursor()

    cur.execute("""
        UPDATE Data_sensor
        SET
            last_watered = COALESCE(?, last_watered),
            image_uri = COALESCE(?, image_uri),
            care_hints = COALESCE(?, care_hints)
        WHERE id = ?
    """, (
        data.get("last_watered"),
        data.get("image_uri"),
        data.get("care_hints"),
        data["id"]
    ))

    con.commit()
    con.close()

def deletePlant(data):
    plant_id = data.get("id")

    if plant_id is None:
        raise ValueError("Missing plant id")

    con = sqlite3.connect(DATABASE_NAME)
    cur = con.cursor()

    cur.execute("""
        DELETE FROM Data_sensor
        WHERE id = ?
    """, (plant_id,))

    con.commit()
    con.close()

def getPlantReference(pid):
    con = sqlite3.connect(DATABASE_NAME)
    con.row_factory = sqlite3.Row
    cur = con.cursor()

    row = cur.execute("""
        SELECT *
        FROM plant_references
        WHERE pid = ?
    """, (pid,)).fetchone()

    con.close()
    return dict(row) if row else None

def searchPlants(search):
    con = sqlite3.connect(DATABASE_NAME)
    con.row_factory = sqlite3.Row
    cur = con.cursor()

    rows = cur.execute("""
        SELECT pid, display_pid, category
        FROM plant_references
        WHERE lower(display_pid) LIKE lower(?)
        LIMIT 50
    """, (f"%{search}%",)).fetchall()

    con.close()

    return [dict(row) for row in rows]

def getAllPlantReferences():
    con = sqlite3.connect(DATABASE_NAME)
    con.row_factory = sqlite3.Row
    cur = con.cursor()

    rows = cur.execute("""
        SELECT pid, display_pid, alias, image, category
        FROM plant_references
        ORDER BY display_pid
    """).fetchall()

    con.close()
    return [dict(row) for row in rows]

def getPlantReferencesPage(limit=10, offset=0):
    con = sqlite3.connect(DATABASE_NAME)
    con.row_factory = sqlite3.Row
    cur = con.cursor()

    rows = cur.execute("""
        SELECT pid, display_pid, alias, image, category
        FROM plant_references
        ORDER BY display_pid
        LIMIT ? OFFSET ?
    """, (limit, offset)).fetchall()

    con.close()
    return [dict(row) for row in rows]

def getAllSensors():
    con = get_connection()
    cur = con.cursor()

    rows = cur.execute("""
        SELECT mac
        FROM Data_sensor
    """).fetchall()

    con.close()

    mac_list = [
        {"mac": row[0]}
        for row in rows
    ]


    return{
            "maccount": len(rows),
            "mac": mac_list
        }


class SimpleHandler(SimpleHTTPRequestHandler):

    def send_json(self, data, status=200):
        response = json.dumps(data, ensure_ascii=False).encode("utf-8") + b"\n"

        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Connection", "close")
        self.end_headers()
        print("JSON:", response)
        self.wfile.write(response)
        self.wfile.flush()

    def send_text(self, text, status=200):
        response = text.encode("utf-8")

        self.send_response(status)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Connection", "close")
        self.end_headers()

        self.wfile.write(response)
        self.wfile.flush()

    def do_POST(self):
        try:
            content_length = int(self.headers.get("Content-Length", 0))
            raw_data = self.rfile.read(content_length)
            data = json.loads(raw_data) if raw_data else {}

            if self.path == "/add_plant":
                addPlant(data)
                self.send_text("Plant added")
                return

            if self.path == "/delete_plant":
                data = json.loads(raw_data)

                deletePlant(data)

                self.send_response(200)
                self.end_headers()
                self.wfile.write(b"Plant deleted")

                return

            if self.path == "/update_plant":

                data = json.loads(raw_data)

                updatePlant(data)

                self.send_response(200)
                self.end_headers()

                self.wfile.write(b"Plant updated")

                return

            saveData(data)

            all_sensors = getAllSensors()


            self.send_json({
                "status": "ok",
                "all_sensors": all_sensors
            })

        except json.JSONDecodeError:
            self.send_text("Invalid JSON", 400)

        except Exception as e:
            print("Server error:", e)
            self.send_text(f"Server Error: {e}", 500)

    def do_GET(self):
        parsed_path = urlparse(self.path)
        query = parse_qs(parsed_path.query)

        if parsed_path.path == "/plants":
            plants = getPlants()

            print("=== PLANTS ===")
            print(json.dumps(plants, indent=2))
            print("==============")

            self.send_json(plants)
            return

        if parsed_path.path == "/sensor":
            mac = query.get("mac", [None])[0]

            if not mac:
                self.send_text("Missing MAC", 400)
                return

            data = getLatestSensorData(mac)

            if data:
                self.send_json(data)
            else:
                self.send_text("No data found", 404)

            return

        if parsed_path.path == "/history":
            mac = query.get("mac", [None])[0]
            limit = int(query.get("limit", [50])[0])

            if not mac:
                self.send_text("Missing MAC", 400)
                return

            self.send_json(getSensorHistory(mac, limit))
            return

        if parsed_path.path == "/new_sensors":
            self.send_json(getAllSensors())
            return

        if parsed_path.path == "/plant_references":
            self.send_json(getAllPlantReferences())
            return

        if parsed_path.path == "/plant_reference":
            pid = query.get("pid", [None])[0]

            if not pid:
                self.send_text("Missing pid", 400)
                return

            data = getPlantReference(pid)

            if data:
                self.send_json(data)
            else:
                self.send_text("Reference not found", 404)

            return
        if parsed_path.path == "/search_plants":

            search = query.get("q", [""])[0]

            self.send_json(searchPlants(search))
            return

        if parsed_path.path == "/plant_references_page":
            limit = int(query.get("limit", [10])[0])
            offset = int(query.get("offset", [0])[0])

            data = getPlantReferencesPage(limit, offset)
            print("Page:", offset, "Rows:", len(data))

            self.send_json(data)
            return

        self.send_text("Endpoint not found", 404)



def startServer():
    server = ThreadingHTTPServer(("0.0.0.0", 8080), SimpleHandler)
    print("Server läuft auf Port 8080...")
    server.serve_forever()


if __name__ == "__main__":
    initDatabase()
    startServer()
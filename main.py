import asyncio
import datetime
from http.server import BaseHTTPRequestHandler, HTTPServer, SimpleHTTPRequestHandler
import json
import sqlite3

#con = sqlite3.connect('MonitorPflanzendaten.db')
#cur = con.cursor()

def main():
    con = sqlite3.connect('MonitorPflanzendaten.db')
    cur = con.cursor()

    print('Check if sensors table exists:')
    tables = cur.execute(
        "SELECT name FROM sqlite_master WHERE type='table' AND name='sensors';"
    ).fetchall()

    if not tables:
        print('Table not found! Creating...')
        cur.execute("CREATE TABLE sensors(mac, name)")
        #con.commit()
    else:
        print('Table found!')
    cur.execute("INSERT INTO sensors(mac, name) VALUES (?, ?)", ("dasdassd","asdasd"))
    con.close()

def saveMac(mac,name):
    con = sqlite3.connect('MonitorPflanzendaten.db')
    cur = con.cursor()
    cur.execute("INSERT INTO Data_sensor(MAC,Pflanzenname) VALUES (?,?) ON CONFLICT (MAC) DO UPDATE SET Pflanzenname = excluded.Pflanzenname", (mac,name))
    con.commit()
    con.close()

def saveData(data):
    con = sqlite3.connect('MonitorPflanzendaten.db')
    cur = con.cursor()
    #sensorID = getSensorID(data["mac"])
    timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    cur.execute("INSERT INTO Data_plants(Timestamp,MAC, Temp,Moisture,Conductivity, Illuminance, JSON) VALUES (?,?,?,?,?,?,?)", (timestamp,data['mac'],data["temp"],data["moisture"],data["conductivity"],data["light"],json.dumps(data)))
    con.commit()
    con.close()

def getSensorID(mac):
    con = sqlite3.connect('MonitorPflanzendaten.db')
    cur = con.cursor()
    sensorID =cur.execute("SELECT MAC FROM Data_Sensor WHERE MAC is (?)",mac)
    con.commit()
    con.close()
    return sensorID


def server():
    class SimpleHandler(SimpleHTTPRequestHandler):
        def do_POST(self):
            content_length = int(self.headers['Content-Length'])
            raw_data = self.rfile.read(content_length)
            print("\nRaw Data:", raw_data)
            try:
                data = json.loads(raw_data)
                print("JSON empfangen:", data)
                #saveMac(data['mac'], "Julius")
                saveData(data)
                self.send_response(200)
                self.end_headers()
                self.wfile.write(b"JSON OK")
            except json.JSONDecodeError:
                self.send_response(400)
                self.end_headers()
                self.wfile.write(b"Invalid JSON")

    srv = HTTPServer(("0.0.0.0", 8080), SimpleHandler)
    print("Server läuft...")
    srv.serve_forever()


if __name__ == "__main__":
    main()
    server()
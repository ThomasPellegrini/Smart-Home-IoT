import paho.mqtt.client as mqtt
import json
import mysql.connector 
from mysql.connector import Error


db = mysql.connector.connect(
	host="INSERT IP",
	user="esp8266",
	passwd="PASSWORD",
	database="assignment1"
)
dbcursor = db.cursor()

def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))

    
    client.subscribe("values")


# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
	print(msg.topic+" "+str(msg.payload))
	data = json.loads(msg.payload)
	print(data["light"]) #è int
	light = data["light"]
	print(type(light))
	try:
		sql = "INSERT INTO `slave` (light) VALUES (%s)" 
		dbcursor.execute(sql, (light,))
		db.commit()
		print(dbcursor.rowcount, "record inserted.")
	except mysql.connector.Error as error:
		print("Failed to insert into MySQL table {}".format(error))



    
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.username_pw_set("","")

client.connect("INSERT IP", n, n)


client.loop_forever()


Start the container
```bash
docker compose -d
```

Create a user and password
```bash
docker run --rm -it -v $(pwd)/mosquitto/config:/mosquitto/config eclipse-mosquitto sh
mosquitto_passwd -c /mosquitto/config/password.txt <username>
exit
```
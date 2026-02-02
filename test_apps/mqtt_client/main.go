package main

import (
	"fmt"
	"log/slog"
	"os"
	"os/signal"
	"syscall"
	"time"

	mqtt "github.com/eclipse/paho.mqtt.golang"
)

const (
	broker            = "tcp://localhost:1883"
	topic             = "go-mqtt/sample"
	clientID          = "go-mqtt-client"
	keepAliveDuration = 2 * time.Second
	pingTimeout       = 1 * time.Second
)

var f = func(logger *slog.Logger) mqtt.MessageHandler {
	return func(client mqtt.Client, msg mqtt.Message) {
		logger.Info("message", "topic", msg.Topic(), "payload", string(msg.Payload()))
	}
}

func main() {
	if len(os.Args) < 3 {
		fmt.Println("Usage: go run main.go <user> <password>")
		os.Exit(1)
	}

	logger := slog.New(slog.NewTextHandler(os.Stdout, &slog.HandlerOptions{
		Level:       slog.LevelDebug,
		ReplaceAttr: nil,
		AddSource:   false,
	}))

	opts := mqtt.NewClientOptions().AddBroker(broker).SetClientID(clientID)
	opts.SetUsername(os.Args[1])
	opts.SetPassword(os.Args[2])
	opts.SetKeepAlive(keepAliveDuration)
	opts.SetDefaultPublishHandler(f(logger.With("component", "mqtt-message-handler")))
	opts.SetPingTimeout(pingTimeout)
	opts.SetConnectionNotificationHandler(func(client mqtt.Client, notification mqtt.ConnectionNotification) {
		l := logger.With("component", "mqtt-connection-notifier")
		switch n := notification.(type) {
		case mqtt.ConnectionNotificationConnected:
			l.Info("connected")
		case mqtt.ConnectionNotificationConnecting:
			l.Info("connecting", "isReconnect", n.IsReconnect, "attempt", n.Attempt)
		case mqtt.ConnectionNotificationFailed:
			l.Info("connection failed", "reason", n.Reason)
		case mqtt.ConnectionNotificationLost:
			l.Info("connection lost", "reason", n.Reason)
		case mqtt.ConnectionNotificationBroker:
			l.Info("broker connection", "broker", n.Broker.String())
		case mqtt.ConnectionNotificationBrokerFailed:
			l.Info("broker connection failed", "reason", n.Reason, "broker", n.Broker.String())
		}
	})

	c := mqtt.NewClient(opts)
	if token := c.Connect(); token.Wait() && token.Error() != nil {
		panic(token.Error())
	}

	if token := c.Subscribe(topic, 0, nil); token.Wait() && token.Error() != nil {
		fmt.Println(token.Error())
		os.Exit(1)
	}

	sc := make(chan os.Signal, 1)
	signal.Notify(sc, os.Interrupt, syscall.SIGTERM)
	<-sc

	if token := c.Unsubscribe(topic); token.Wait() && token.Error() != nil {
		fmt.Println(token.Error())
		os.Exit(1)
	}

	c.Disconnect(250)
}

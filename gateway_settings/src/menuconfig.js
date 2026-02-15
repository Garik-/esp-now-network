export const menuConfig = Object.freeze({
  menu: 'Settings',
  config: [
    {
      legend: 'Wi-Fi',
      items: [
        {
          key: 'wifi.ssid',
          title: 'SSID',
          type: 'text',
          default: 'myssid',
          help: 'SSID (network name) for the device to connect to',
        },
        {
          key: 'wifi.password',
          title: 'Password',
          type: 'password',
          default: 'mypassword',
          help: 'WiFi password (WPA or WPA2) for the device to use. Can be left blank if the network has no security set',
        },
        {
          key: 'wifi.channel',
          title: 'Channel',
          type: 'number',
          default: 6,
          range: [0, 14],
          help: 'The channel on which sending and receiving ESP-NOW data',
        },
      ],
    },
    {
      legend: 'HTTP',
      items: [
        /*{
          key: 'http.port',
          title: 'Server Port',
          type: 'number',
          default: 80,
          range: [1, 65535],
          help: 'Port number for the HTTP server',
        },*/

        {
          key: 'http.auth.user',
          title: 'Username',
          type: 'text',
          default: 'admin',
          help: 'Username for Basic Auth',
        },
        {
          key: 'http.auth.password',
          title: 'Password',
          type: 'password',
          default: 'admin',
          help: 'Password for Basic Auth',
        },
      ],
    },
    {
      legend: 'MQTT',
      items: [
        {
          key: 'mqtt.uri',
          title: 'URI',
          type: 'url',
          default: 'mqtt://localhost:1883',
          help: 'URL of the MQTT broker to which the gateway will connect',
        },
        {
          key: 'mqtt.user',
          title: 'Username',
          type: 'text',
          default: 'mqtt_user',
          help: 'Username for authenticating with the MQTT broker',
        },
        {
          key: 'mqtt.password',
          title: 'Password',
          type: 'password',
          default: 'mqtt_password',
          help: 'Password for authenticating with the MQTT broker',
        },
      ],
    },
  ],
});

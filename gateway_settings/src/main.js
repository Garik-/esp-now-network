import './style.css';

const menuConfig = Object.freeze({
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
          help: 'The channel on which sending and receiving ESPNOW data',
        },
      ],
    },
    {
      legend: 'HTTP',
      items: [
        {
          key: 'http.port',
          title: 'Server Port',
          type: 'number',
          default: 80,
          range: [1, 65535],
          help: 'Port number for the HTTP server',
        },
        {
          key: 'http.auth.user',
          title: 'Username',
          type: 'text',
          default: '',
          help: 'Username for Basic Auth on HTTP POST config endpoints. Leave blank to disable Basic Auth',
        },
        {
          key: 'http.auth.password',
          title: 'Password',
          type: 'password',
          default: '',
          help: 'Password for Basic Auth on HTTP POST config endpoints. Leave blank to disable Basic Auth',
        },
      ],
    },
    {
      legend: 'MQTT',
      items: [
        {
          key: 'mqtt.uri',
          title: 'MQTT Broker URL',
          type: 'url',
          default: 'mqtt://localhost:1883',
          help: 'URL of the MQTT broker to which the gateway will connect',
        },
        {
          key: 'mqtt.user',
          title: 'MQTT Broker Username',
          type: 'text',
          default: 'mqtt_user',
          help: 'Username for authenticating with the MQTT broker',
        },
        {
          key: 'mqtt.password',
          title: 'MQTT Broker Password',
          type: 'password',
          default: 'mqtt_password',
          help: 'Password for authenticating with the MQTT broker',
        },
      ],
    },
  ],
});

function convertToPath(str) {
  const parts = str.split('.');
  return '/config/' + parts.join('/');
}

document.getElementById('menu-title').textContent = menuConfig.menu;
const mainEl = document.getElementById('main');

function setValue(input, defaultValue = '') {
  fetch(convertToPath(input.id))
    .then((response) => {
      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`);
      }
      return response.text(); 
    })
    .then((text) => {
      input.value = text;
    })
    .catch((err) => {
      console.error(err);
      input.value = defaultValue;
    });
}

function createItem(menu) {
  const div = document.createElement('div');
  div.className = 'field border label';

  const label = document.createElement('label');
  label.textContent = menu.title;
  label.setAttribute('for', menu.key);

  const output = document.createElement('output');
  output.textContent = menu.help;

  const input = document.createElement('input');
  input.name = input.id = menu.key;
  input.required = true;
  // input.placeholder = menu.default || '';
  input.setAttribute('type', menu.type);
  // input.type = menu.type;

  input.addEventListener('input', () => {
    if (input.matches(':invalid')) {
      div.classList.add('invalid');
      output.classList.add('invalid');
      output.textContent = 'That doesn\’t look right';
    } else {
      div.classList.remove('invalid');
      output.classList.remove('invalid');
      output.textContent = menu.help;
    }
  });

  if (menu.type === 'number') {
    input.min = menu.range[0];
    input.max = menu.range[1];
  }

  let i;

  if (menu.type === 'password') {
    div.classList.add('suffix');
    i = document.createElement('i');
    i.className = 'front';
    i.textContent = 'visibility';
  }

  setValue(input, menu.default);

  div.appendChild(input);
  div.appendChild(label);

  if (i) {
    div.appendChild(i);
  }

  div.appendChild(output);

  return div;
}

menuConfig.config.forEach((menu) => {
  if ('legend' in menu) {
    const fieldset = document.createElement('fieldset');
    const legend = document.createElement('legend');
    legend.textContent = menu.legend;
    fieldset.appendChild(legend);

    menu.items.forEach((item) => {
      fieldset.appendChild(createItem(item));
    });

    mainEl.appendChild(fieldset);

    return true;
  }

  mainEl.appendChild(createItem(menu));
});

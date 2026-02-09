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

const inputElements = [];
const cacheValues = {};

function convertToPath(str) {
  const parts = str.split('.');
  return '/config/' + parts.join('/');
}

class AuthSession {
  constructor(key = 'auth') {
    this.key = key;
  }

  credentials(user, password) {
    if (!user || !password) throw new Error('User and password required');
    const token = btoa(user + ':' + password);
    sessionStorage.setItem(this.key, token);
  }

  get token() {
    return sessionStorage.getItem(this.key);
  }

  clear() {
    return sessionStorage.removeItem(this.key);
  }
}

const auth = new AuthSession();

function setValue(input, defaultValue = '') {
  fetch(convertToPath(input.name), {
    headers: {
      Authorization: 'Basic ' + auth.token,
    },
  })
    .then((response) => {
      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`);
      }
      return response.text();
    })
    .then((text) => {
      input.value = text;
      cacheValues[input.name] = text;
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
  input.type = menu.type;

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

  inputElements.push(input);

  return div;
}

function createHeader() {
  const header = document.createElement('header');
  header.id = 'main-header';
  const nav = document.createElement('nav');
  const backBtn = document.createElement('button');
  backBtn.className = 'circle transparent';
  backBtn.innerHTML = '<i>arrow_back</i>';

  backBtn.addEventListener('click', () => {
    auth.clear();
    render();
  });

  nav.appendChild(backBtn);

  const h6 = document.createElement('h6');
  h6.textContent = menuConfig.menu;
  h6.className = 'max';

  nav.appendChild(h6);

  const saveBtn = document.createElement('button');
  saveBtn.className = 'transparent';
  saveBtn.textContent = 'SAVE';

  saveBtn.addEventListener('click', () => {
    inputElements.forEach((input) => {
      if (cacheValues[input.name] === input.value) {
        return true;
      }

      console.log('value change', input.name);
    });
  });

  nav.appendChild(saveBtn);

  header.appendChild(nav);

  document.body.prepend(header);
}

function createForm(mainEl) {
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
}

function editPage(mainEl) {
  createHeader();
  createForm(mainEl);
}

function loginPage(mainEl) {
  const article = document.createElement('article');
  article.className =
    'absolute center bottom top-round no-bottom-round large-padding';

  const form = document.createElement('form');
  form.className = 'grid large-space';
  form.innerHTML = `<div class="s12">
                <h5>Welcome back!</h5>
            </div>
            <div class="s12">
                <div class="field label border">
                    <input name="username" id="username" required type="text">
                    <label for="username">Username</label>
                </div>

                <div class="field label suffix border">
                    <input name="password" id="password" required type="password">
                    <label for="password">Password</label>
                    <i class="front">visibility</i>
                </div>
            </div>
            <div class="s12">
                <button class="responsive small-round large no-side" type="submit">Sign in</button>
            </div>
            
            `;

  form.addEventListener('submit', (e) => {
    e.preventDefault();
    const data = new FormData(e.target);
    auth.credentials(data.get('username'), data.get('password'));

    render();
  });

  article.appendChild(form);

  mainEl.appendChild(article);
}

function render() {
  document.body.replaceChildren();
  const main = document.createElement('main');
  main.className = 'responsive';

  if (!auth.token) {
    loginPage(main);
  } else {
    editPage(main);
  }

  document.body.appendChild(main);
}

render();

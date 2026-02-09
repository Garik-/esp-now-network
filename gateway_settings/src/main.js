import './style.css';

import { menuConfig } from './menuconfig';

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
      output.textContent = "That doesn't look right";
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

  div.append(input, label, i, output);

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

  const h6 = document.createElement('h6');
  h6.textContent = menuConfig.menu;
  h6.className = 'max';

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

  nav.append(backBtn, h6, saveBtn);
  header.append(nav);

  document.body.prepend(header);
}

function createForm() {
  const form = document.createElement('form');

  menuConfig.config.forEach((menu) => {
    if ('legend' in menu) {
      const fieldset = document.createElement('fieldset');
      const legend = document.createElement('legend');
      legend.textContent = menu.legend;
      fieldset.append(legend);

      menu.items.forEach((item) => {
        fieldset.append(createItem(item));
      });

      form.append(fieldset);

      return true;
    }

    form.append(createItem(menu));
  });

  return form;
}

function editPage(mainEl) {
  createHeader();
  mainEl.append(createForm());
}

function loginTemplate() {
  return `<div class="s12"><h5>Welcome back!</h5></div><div class="s12"><div class="field label border"><input name="username" id="username" required type="text"><label for="username">Username</label></div><div class="field label suffix border"><input name="password" id="password" required type="password"><label for="password">Password</label><i class="front">visibility</i></div></div><div class="s12"><button class="responsive small-round large no-side" type="submit">Sign in</button></div>`;
}

function loginPage(mainEl) {
  const article = document.createElement('article');
  article.className =
    'absolute center bottom top-round no-bottom-round large-padding';

  const form = document.createElement('form');
  form.className = 'grid large-space';
  form.innerHTML = loginTemplate();

  form.addEventListener('submit', (e) => {
    e.preventDefault();
    const data = new FormData(e.target);
    auth.credentials(data.get('username'), data.get('password'));

    render();
  });

  article.append(form);

  mainEl.append(article);
}

function render() {
  for (const key in cacheValues) delete cacheValues[key];

  document.body.replaceChildren();
  const main = document.createElement('main');
  main.className = 'responsive';

  if (!auth.token) {
    document.body.classList.add('animated-gradient');
    loginPage(main);
  } else {
    document.body.classList.remove('animated-gradient');
    editPage(main);
  }

  document.body.append(main);
}

render();

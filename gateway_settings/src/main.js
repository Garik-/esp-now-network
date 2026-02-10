import './style.css';

import { AuthSession } from './auth';

import LoginPage from './page/login';
import EditPage from './page/edit';

const auth = new AuthSession();

function backHandler(e) {
  e.preventDefault();
  auth.clear();
  render();
}

function render() {
  document.body.replaceChildren();
  const main = document.createElement('main');
  main.className = 'responsive';

  if (!auth.token) {
    document.body.classList.add('animated-gradient');
    LoginPage({ main, auth, resolve: render });
  } else {
    document.body.classList.remove('animated-gradient');
    EditPage({ main, auth, backHandler });
  }

  document.body.append(main);

  if (typeof ui === 'function') {
    ui(); //eslint-disable-line
  }
}

render();

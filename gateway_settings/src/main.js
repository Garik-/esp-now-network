import './style.css';

import { AuthSession } from './auth';

import loginPage from './page/login';
import editPage from './page/edit';

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
    loginPage({ main, auth, resolve: render });
  } else {
    document.body.classList.remove('animated-gradient');
    editPage({ main, auth, backHandler });
  }

  document.body.append(main);

  if (typeof ui === 'function') {
    ui(); //eslint-disable-line
  }
}

render();

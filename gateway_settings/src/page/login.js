import { AUTH_URL } from '../const';

function loginTemplate() {
  return `<div class="s12"><h5>Welcome back!</h5></div><div class="s12"><div class="field label border"><input name="username" id="username" required type="text"><label for="username">Username</label></div><div class="field label suffix border"><input name="password" id="password" required type="password"><label for="password">Password</label><i class="front">visibility</i></div></div><div class="s12"><button class="responsive small-round large no-side" type="submit">Sign in</button></div>`;
}

export default function ({ main, auth, resolve }) {
  if (!main || !auth || !resolve) {
    throw new Error('missing required props');
  }

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

    fetch(AUTH_URL, {
      headers: auth.headers(),
    })
      .then((response) => {
        if (!response.ok) {
          throw new Error(`HTTP error! status: ${response.status}`);
        }
        return response.text();
      })
      .then(resolve)
      .catch((err) => {
        console.error(err);
        auth.clear();

        document
          .getElementById('password')
          .parentElement.classList.add('invalid');
      });
  });

  article.append(form);
  main.append(article);
}

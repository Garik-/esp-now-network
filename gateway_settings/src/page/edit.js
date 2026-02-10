import Handler from '../components/header';
import Form from '../components/form';

const FORM_ID = 'settings-form';

function saveHandler(auth) {
  return function (e) {
    e.preventDefault();
    const data = new FormData(document.getElementById(FORM_ID));

    for (var pair of data.entries()) {
      if (cacheValues[pair[0]] === pair[1]) {
        continue;
      }

      fetch(convertToPath(pair[0]), {
        method: 'POST',
        headers: {
          Authorization: 'Basic ' + auth.token,
        },
        body: pair[1],
      });

      console.log('value change', pair);
    }
  };
}

const cacheValues = {};

function convertToPath(str) {
  const parts = str.split('.');
  return '/config/' + parts.join('/');
}

async function setValue(initPromise, input, defaultValue = '') {
  await initPromise;
  input.value = cacheValues[input.id] ?? defaultValue;
}

export default function ({ main, auth, backHandler }) {
  if (!main || !auth || !backHandler) {
    throw new Error('missing required props');
  }

  Handler({ backHandler, saveHandler: saveHandler(auth), title: 'Settings' });

  const initPromise = (async () => {
    for (const key in cacheValues) delete cacheValues[key];

    const resp = await fetch('/settings.csv', {
      headers: {
        Authorization: 'Basic ' + auth.token,
      },
    });
    if (!resp.ok) {
      throw new Error(`HTTP ${resp.status}`);
    }

    const text = await resp.text();

    for (const line of text.split('\n')) {
      const trimmed = line.trim();
      if (!trimmed || trimmed.startsWith('#')) continue;

      const [key, value] = trimmed.split('=', 2);
      cacheValues[key] = value;
    }
  })();

  main.append(Form({ initPromise, id: FORM_ID, setValue }));
}

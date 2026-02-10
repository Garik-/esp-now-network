import { menuConfig } from '../menuconfig';

function createItem({ initPromise, menu, setValue }) {
  if (!initPromise || !menu || !setValue) {
    throw new Error('missing required props');
  }

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

  let i = '';

  if (menu.type === 'password') {
    div.classList.add('suffix');
    i = document.createElement('i');
    i.className = 'front';
    i.textContent = 'visibility';
  }

  setValue(initPromise, input, menu.default);

  div.append(input, label, i, output);

  return div;
}

export default function ({ initPromise, id, setValue }) {
  if (!initPromise || !id || !setValue) {
    throw new Error('missing required props');
  }

  const form = document.createElement('form');
  form.id = id;

  menuConfig.config.forEach((menu) => {
    if ('legend' in menu) {
      const fieldset = document.createElement('fieldset');
      const legend = document.createElement('legend');
      legend.textContent = menu.legend;
      fieldset.append(legend);

      menu.items.forEach((item) => {
        fieldset.append(createItem({ initPromise, menu: item, setValue }));
      });

      form.append(fieldset);

      return true;
    }

    form.append(createItem({ initPromise, menu, setValue }));
  });

  return form;
}

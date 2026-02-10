export default function ({ backHandler, saveHandler, title }) {
  if (!backHandler || !saveHandler || !title) {
    throw new Error('missing required props');
  }

  const header = document.createElement('header');
  header.id = 'main-header';

  const nav = document.createElement('nav');

  const backBtn = document.createElement('button');
  backBtn.className = 'circle transparent';
  backBtn.innerHTML = '<i>arrow_back</i>';
  backBtn.addEventListener('click', backHandler);

  const h6 = document.createElement('h6');
  h6.textContent = title;
  h6.className = 'max';

  const saveBtn = document.createElement('button');
  saveBtn.className = 'transparent';
  saveBtn.textContent = 'SAVE';
  saveBtn.addEventListener('click', saveHandler);

  nav.append(backBtn, h6, saveBtn);
  header.append(nav);

  document.body.prepend(header);
}

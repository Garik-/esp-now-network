export class AuthSession {
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

  headers() {
    const token = this.token;
    return token ? { Authorization: `Basic ${token}` } : {};
  }

  clear() {
    return sessionStorage.removeItem(this.key);
  }
}

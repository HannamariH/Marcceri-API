# Marcceri-api
Pohjana Kansalliskirjaston Usemarcon-projekti https://github.com/NatLibFi/usemarcon. Samaan projektiin on luotu node.js-projekti (lähinnä index.js), joka ottaa vastaan marc-tiedostoja, konvertoi ne oikeaan muotoon Usemarconia käyttäen ja tallentaa kirjastojärjestelmä Kohaan.

Tuotantoon tarvitaan .env-tiedosto ja basic authorization muuttujaan BASIC.

config.json-tiedostossa luetellaan sähköpostiosoitteet, joilla on pääsy Marcceriin (oletuksena Shibboleth-kirjautuminen, joka asettaa käyttäjän sähköpostiosoitteen HTTP-pyyntöjen headeriin).
## Käynnistys Dockerilla
```docker buildx build --platform linux/amd64 -t marcceri .```

```docker run -d --name marcceri -p <port>:3000 marcceri```
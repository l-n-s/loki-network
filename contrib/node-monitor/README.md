# Building / Running

after building lokinet

```
cd contrib/node-monitor
npm install
npm start
```

and wait 30s for final score

# Scoring

Lokinet gets a point for everything it does correctly.

Currently:
- succesful ping to snode (1 per second after a snode is discovered)
- session with snode established
- got a router from exploration
- Hanlding DHT S or R message
- a path is built
- obtained an exit via
- granted exit
- IntroSet publish confirmed
- utp.session.sendq. has a min zero count (no blocked buffers on routers)

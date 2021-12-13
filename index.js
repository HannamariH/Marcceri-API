'use strict'

const Koa = require('koa')
const Router = require('koa-router')
const bodyParser = require('koa-body')

const app = new Koa()
const router = new Router()

app.use(bodyParser({
    multipart: true,
    urlencoded: true
}))

router.get("/convert", (ctx) => {
    console.log("converted!")

    const { exec } = require("child_process")
    exec("/usemarcon/bin/usemarcon /usemarcon/emerald.ini /usemarcon/aamulehti.xml /usemarcon/testi-output.xml", (error, stdout, stderr) => {
    if (error) {
        console.log("error: " + error.message)
        return
    }
    if (stderr) {
        console.log("stderr: " + stderr)
        return
    }
    console.log("stdout: " + stdout)
    })

    ctx.body = "Hello Convert!"
    ctx.status = 200
})

router.get("/", (ctx) => {
    ctx.body = "Hello World!"
})

app.use(router.routes())

module.exports = app

const PORT = process.env.PORT || 3000
app.listen(PORT, () => console.log(`running on port ${PORT}`))
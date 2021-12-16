'use strict'

const Koa = require('koa')
const Router = require('koa-router')
const multer = require('@koa/multer')

const app = new Koa()
const router = new Router()

//Upload File Storage Path and File Naming
const storage = multer.diskStorage({
    destination: function (req, file, cb) {
        cb(null, '/usemarcon/uploads')
    },
    //TODO: tähän tarkistus, mikä on tiedostopääte/-tyyppi
    filename: function (req, file, cb) {
        cb(null, "input.xml")
    }
})
//TODO: onko nämä tarpeen? Kopioitu mallista.
const limits = {
    fields: 10,//Number of non-file fields
    fileSize: 500 * 1024,//File Size Unit b
    files: 1//Number of documents
}
const upload = multer({storage,limits})


router.post('/convert', upload.single('file'), async (ctx)=>{

    console.log('ctx.request.body.ini:', ctx.request.body.ini)

    const { exec } = require("child_process")
    exec(`/usemarcon/bin/usemarcon /usemarcon/${ctx.request.body.ini} /usemarcon/uploads/input.xml /usemarcon/output.xml`, (error, stdout, stderr) => {
    if (error) {
        console.log("error: " + error.message)
        ctx.body = {
            data: ctx.file,
            error: error.message
        }
    }
    if (stderr) {
        console.log("stderr: " + stderr)
        ctx.body = {
            data: ctx.file,
            stderr: stderr
        }
    }
    console.log("stdout: " + stdout)
    ctx.body = {
        data: ctx.file,
        stdout: stdout
    }
    })

    ctx.body = {
        data: "viimeinen vara"
    }
    
})

app.use(router.routes()).use(router.allowedMethods())

module.exports = app

const PORT = process.env.PORT || 3000
app.listen(PORT, () => console.log(`running on port ${PORT}`))
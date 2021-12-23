'use strict'

const Koa = require('koa')
const Router = require('koa-router')
const multer = require('@koa/multer')
const { chunksToLinesAsync } = require('@rauschma/stringio');
const { exec } = require('child_process');

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

const streamToString = async (readable) => {
    let fullStdout = ""
    for await (const line of chunksToLinesAsync(readable)) {
        fullStdout += line    
    }
    return fullStdout
}

//-----------routes--------------------------------

router.post('/convert', upload.single('file'), async (ctx)=>{

    console.log('ctx.request.body.ini:', ctx.request.body.ini)

    const output = exec(`/usemarcon/bin/usemarcon /usemarcon/${ctx.request.body.ini} /usemarcon/uploads/input.xml /usemarcon/output.xml`)

    const result = await streamToString(output.stdout)

    return ctx.body = {
        data: result
    }

    /*exec(`/usemarcon/bin/usemarcon /usemarcon/${ctx.request.body.ini} /usemarcon/uploads/input.xml /usemarcon/output.xml`, (error, stdout, stderr) => {
    if (error) {
        console.log("error: " + error.message)
        return ctx.body = {
            data: ctx.file,
            error: error.message
        }
    }
    if (stderr) {
        console.log("stderr: " + stderr)
        return ctx.body = {
            data: ctx.file,
            stderr: stderr
        }
    }
    console.log("stdout: " + stdout)
    return ctx.body = {
        data: ctx.file,
        stdout: stdout
    }
    })

    return ctx.body = {
        data: "viimeinen vara"
    }*/
    
})



app.use(router.routes()).use(router.allowedMethods())

module.exports = app

const PORT = process.env.PORT || 3000
app.listen(PORT, () => console.log(`running on port ${PORT}`))
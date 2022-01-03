'use strict'

const Koa = require('koa')
const Router = require('koa-router')
const multer = require('@koa/multer')
const { exec } = require('child_process')
const fs = require("fs")

const app = new Koa()
const router = new Router()

//Upload File Storage Path and File Naming
const storage = multer.diskStorage({
    destination: function (req, file, cb) {
        cb(null, '/usemarcon/uploads')
    },
    //TODO: tähän tarkistus, mikä on tiedostopääte/-tyyppi
    //.mrc näkyy toimivan myös, vaikka pääte on xml
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
const upload = multer({ storage, limits })

const streamToString = async (readable) => {

    const chunks = []

    for await (const chunk of readable) {
        chunks.push(Buffer.from(chunk))
    }

    return Buffer.concat(chunks).toString("utf-8")
}

const getTitles = () => {
    const file = fs.readFileSync("/usemarcon/uploads/input.xml", "utf-8")

    const titleFieldRegex = /<datafield tag="245"((.|\s)*?)<\/datafield>/g
    const beforeTitleRegex = /<datafield tag="245"((.|\s)*?)<subfield code="a">/g
    const afterTitleRegex = /<\/subfield>((.|\s)*?)$/g

    const datafields = file.match(titleFieldRegex)

    const titles = []    

    for (let field of datafields) {
        let strippedField = field.replace(beforeTitleRegex, "").replace(afterTitleRegex, "")
        titles.push(strippedField)
    }

    return titles
}

//-----------routes--------------------------------

router.post('/convert', upload.single('file'), async (ctx) => {

    //TODO: voiko tässä vielä poistaa edellisen input.xml:n? Tai onko tarvetta?
    //TODO: tsekkaa, onko zip ja unzippaa

    let output = ""
    try {
        output = exec(`/usemarcon/bin/usemarcon /usemarcon/${ctx.request.body.ini} /usemarcon/uploads/input.xml /usemarcon/output.xml`)
        let result = await streamToString(output.stdout)

        result = result.split("Conversion progress:")[1]

        //TODO: resultille parsimista, erroreista ja warningeista riippuen erilaiset responset

        const titles = getTitles()

        return ctx.body = {
            data: result,
            titles: titles
        }
    } catch (error) {
        console.log(error)
        ctx.status = 500
        return ctx.body = {
            error: error.message
        }
    }

})

//postaa output.xml-tiedoston tietueet Kohaan, yksi kerrallaan
router.post("/toKoha", async (ctx) => {
    //TODO: tarkista, että on olemassa output.xml, josta voi poimia tietueet lähetettäväksi
    //(miten tarkistetaan, että output.xml on tuore?)
})


app.use(router.routes()).use(router.allowedMethods())

module.exports = app

const PORT = process.env.PORT || 3000
app.listen(PORT, () => console.log(`running on port ${PORT}`))
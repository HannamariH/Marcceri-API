'use strict'

const Koa = require('koa')
const cors = require("@koa/cors")
const Router = require('koa-router')
const multer = require('@koa/multer')
const { exec } = require('child_process')
const fs = require("fs")
const axios = require('axios')
require('dotenv').config()

const app = new Koa()
app.use(cors())
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
/*const limits = {
    fields: 10,//Number of non-file fields
    fileSize: 500 * 1024,//File Size Unit b
    files: 1//Number of documents
}*/
const upload = multer({ storage/*, limits*/ })

const streamToString = async (readable) => {

    const chunks = []

    for await (const chunk of readable) {
        chunks.push(Buffer.from(chunk))
    }

    return Buffer.concat(chunks).toString("utf-8")
}

const getTitles = () => {
    const file = fs.readFileSync("/usemarcon/output.xml", "utf-8")

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

const splitOutput = () => {
    //TODO: tarkista, että output.xml on olemassa (try-catch?)
    const file = fs.readFileSync("/usemarcon/output.xml", "utf-8")
    const strippedFile = file.replace(/((.|\s)*?)<collection((.|\s)*?)>/, "").replace("</collection>", "").trim()
    let records = strippedFile.split("</record>")
    //remove empty elements
    records = records.filter(Boolean)
    //add closing tag back
    records = records.map(record => record.concat("</record>"))
    return records
}

//-----------routes--------------------------------

router.post('/convert', upload.single('file'), async (ctx) => {

    //TODO: voiko tässä vielä poistaa edellisen input.xml:n? Tai onko tarvetta?
    //TODO: tsekkaa, onko zip ja unzippaa

    let output = ""
    try {
        output = exec(`/usemarcon/bin/usemarcon /usemarcon/${ctx.request.body.ini} /usemarcon/uploads/input.xml /usemarcon/output.xml`)
        let result = await streamToString(output.stdout)
        console.log(result)

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
router.post("/tokoha", async (ctx) => {

    //TODO: tarkista, että on olemassa output.xml, josta voi poimia tietueet lähetettäväksi
    //(miten tarkistetaan, että output.xml on tuore?)

    const records = splitOutput()

    let biblionumbers = []

    for (const record of records) {
        //post to koha
        await axios({
            method: "POST",
            data: record,
            url: "https://app1.jyu.koha.csc.fi/api/v1/contrib/natlibfi/biblios",
            headers: {
                'Content-Type': 'text/xml',
                'Authorization': `Basic ${process.env.BASIC}`
            }
        }).then((response) => {
            console.log(response.data.biblio_id)
            biblionumbers.push(response.data.biblio_id)
            console.log("biblionumbers: ", biblionumbers)
        }).catch(error => {
            console.log(error)
            //TODO: break, ei enää postata uusia tietueita?
            //TODO: ilmoita käyttäjälle, että epäonnistui (xml tai siitä nimeke?)
        })
    }

    ctx.body = {
        biblionumbers: biblionumbers
    }
})


app.use(router.routes()).use(router.allowedMethods())

module.exports = app

const PORT = process.env.PORT || 3000
app.listen(PORT, () => console.log(`running on port ${PORT}`))
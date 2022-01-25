'use strict'

const Koa = require('koa')
const cors = require("@koa/cors")
const Router = require('koa-router')
const multer = require('@koa/multer')
const bodyParser = require("koa-bodyparser")
const { exec } = require('child_process')
const fs = require("fs")
const AmdZip = require("adm-zip")
const path = require("path")
const axios = require('axios')
require('dotenv').config()

const app = new Koa()
app.use(cors())
app.use(bodyParser())
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

const unzipFiles = () => {
    const zip = new AmdZip("/usemarcon/uploads/input.xml")
    fs.rmSync("/usemarcon/uploads/extracted", { recursive: true, force: true })
    const outputDir = "/usemarcon/uploads/extracted"
    zip.extractAllTo(outputDir)
    const files = fs.readdirSync(outputDir)
    files.forEach((file) => {
        const extension = file.split(".").pop()
        if (extension === "mrc" | extension === "xml") {
            fs.renameSync(`/usemarcon/uploads/extracted/${file}`, "/usemarcon/uploads/input.xml")
        }
    })
}

//-----------routes--------------------------------

router.post('/convert', upload.single('file'), async (ctx) => {

    //TODO: voiko tässä vielä poistaa edellisen input.xml:n? Tai onko tarvetta?

    if (ctx.request.body.filetype === "application/zip") {
        try {
            unzipFiles()
        } catch (error) {
            ctx.status = 500
            return ctx.body = {
                error: "Unzipping failed"
            }
        }
    }

    let output = ""
    try {
        output = exec(`/usemarcon/bin/usemarcon /usemarcon/${ctx.request.body.ini} /usemarcon/uploads/input.xml /usemarcon/output.xml`)
        let result = await streamToString(output.stdout)
        console.log(result)

        //TODO: onko vaara, että aina ei ole "100%" tuloksessa? jos on epäkelpoja tietueita?
        result = result.split("100% ")[1]

        //result = "Converted records: " + result

        //TODO: resultille parsimista, erroreista ja warningeista riippuen erilaiset responset?

        let titles = []
        //jos tiedosto on väärässä muodossa, datafieldsejä ei välttämättä löydy
        //silloin TypeError: datafields is not iterable
        try {
            titles = getTitles()
        } catch (TypeError) {
            ctx.status = 500
            return ctx.body = {
                error: TypeError.message
            }
        }
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

    const bibliosToPost = ctx.request.body

    const records = splitOutput()

    let biblionumbers = []

    //onhan records.length === bibliosToPost.length??

    for (let i = 0; i < records.length; i++) {
        if (bibliosToPost[i]) {
            await axios({
                method: "POST",
                data: records[i],
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
    }

    ctx.body = {
        biblionumbers: biblionumbers
    }
})


app.use(router.routes()).use(router.allowedMethods())

module.exports = app

const PORT = process.env.PORT || 3000
app.listen(PORT, () => console.log(`running on port ${PORT}`))
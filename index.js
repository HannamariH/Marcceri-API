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
const config = require("./config.json")
const winston = require('winston')

const app = new Koa()
app.use(cors())
app.use(bodyParser())

app.use(async (ctx, next) => {
    //TODO: alla oleva rivi käyttöön tuotannossa
    //if (config.users.includes(ctx.request.headers.mail)) {
    if (config.users.includes("hannamari.h.heiniluoma@jyu.fi")) {
        console.log("käyttäjä sallittu")
        ctx.status = 200
        await next()
    } else {
        console.log("access denied for user: ", ctx.request.headers.mail)
        logger.error({
            message: "access denied",
            user: ctx.request.headers.mail
        })
        ctx.status = 403
        ctx.body = { error: "Sinulla ei ole oikeutta Marccerin käyttöön." }
    }
})
const router = new Router()

//Upload File Storage Path and File Naming
const storage = multer.diskStorage({
    destination: function (req, file, cb) {
        cb(null, '/usemarcon/uploads')
    },
    filename: function (req, file, cb) {
        cb(null, "input.xml")
    }
})

const upload = multer({ storage })

const logger = winston.createLogger({
    format: winston.format.combine(
        winston.format.timestamp(),
        winston.format.prettyPrint()
    ),
    transports: [
        new winston.transports.File({
            filename: "./logs/error.log",
            level: "error"
        }),
        new winston.transports.File({
            filename: "./logs/combined.log",
            level: "info"
        })
    ]
})

const streamToString = async (readable) => {
    console.log("streamtostringissä")

    const chunks = []

    for await (const chunk of readable) {
        console.log("chunk", Buffer.from(chunk).toString("utf-8"))
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

//oletus, että zipissä on vain yksi tiedosto!
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

const getRecordsWithErrors = (conversionMessage) => {
    const errorRegex = /ERROR (.*).*(record \d*,.*)/g
    const errors = conversionMessage.match(errorRegex)
    let recordNumbers = []
    for (const error of errors) {
        const errorNumber = error.replace(/.*record\s/, "").replace(/,.*\)/, "").trim()
        recordNumbers.push(errorNumber)
    }
    return recordNumbers
}

//-----------routes--------------------------------

router.get("/auth", async (ctx, next) => {
    await next()
})

router.post('/convert', upload.single('file'), async (ctx) => {

    //TODO: voiko tässä vielä poistaa edellisen input.xml:n? Tai onko tarvetta?

    if (ctx.request.body.filetype === "application/zip") {
        try {
            unzipFiles()
        } catch (error) {
            logger.error({
                message: "Unzipping failed",
                file: ctx.request.body.filename
            })
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

        let errors = true

        if (!result.includes("ERROR") && !result.includes("WARNING")) {
            //TODO: onko vaara, että aina ei ole "100%" tuloksessa? jos on epäkelpoja tietueita?
            result = result.split("100% ")[1]
            console.log("result: ", result)
            //tarviiko WARNINGeista välittää tässä? errors = false vaikka WARNINGeja oliskin?
            errors = false
        }

        let errorRecords = []
        if (errors) {
            errorRecords = getRecordsWithErrors(result)
        }
        console.log("errorRecords", errorRecords)
        //result = "Converted records: " + result

        //TODO: resultille parsimista, erroreista ja warningeista riippuen erilaiset responset?

        let titles = []
        //jos tiedosto on väärässä muodossa, datafieldsejä ei välttämättä löydy
        //silloin TypeError: datafields is not iterable
        try {
            titles = getTitles()
        } catch (TypeError) {
            logger.error({
                message: "TypeError, tiedosto väärässä muodossa.",
                file: ctx.request.body.filename
            })
            ctx.status = 500
            return ctx.body = {
                error: TypeError.message
            }
        }
        return ctx.body = {
            conversionMessage: result,
            titles: titles,
            errorRecords: errorRecords
        }
    } catch (error) {
        console.log(error)
        logger.error({
            message: error.message
        })
        ctx.status = 500
        return ctx.body = {
            error: error.message,
            file: ctx.request.body.filename
        }
    }

})

//postaa output.xml-tiedoston tietueet Kohaan, yksi kerrallaan
router.post("/tokoha", async (ctx) => {

    //TODO: KOKO REITIN PITÄÄ PERUSTUA ENEMMÄN TIETUEIDEN NIMILLE! Ne myös lokituksiin

    //TODO: tarkista, että on olemassa output.xml, josta voi poimia tietueet lähetettäväksi
    //(miten tarkistetaan, että output.xml on tuore?)

    const bibliosToPost = ctx.request.body

    const records = splitOutput()

    let biblionumbers = []
    let kohaError = ""

    //onhan records.length === bibliosToPost.length??
    //TODO: ei ole, jos on tullut virheitä! (esim. 3_virheita.xml)

    console.log("records.length", records.length)
    console.log("bibliosToPost.length", bibliosToPost.length)

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
                console.log(error.response.data)
                kohaError = `Tietueen ${[i + 1]} tallentaminen Kohaan epäonnistui`
            })
        }
        if (kohaError) break
    }
    if (kohaError) {
        //TODO: kohaErroriin tietueen nimi!
        logger.error({
            käyttäjä: ctx.request.headers.mail,
            kohaError: kohaError
        })
        ctx.status = 500
        return ctx.body = {
            error: kohaError
        }
    }
    logger.info({
        message: "Tallennettu Kohaan",
        käyttäjä: ctx.request.headers.mail,
        tietueet: biblionumbers
    })
    ctx.body = {
        biblionumbers: biblionumbers
    }
})


app.use(router.routes()).use(router.allowedMethods())

module.exports = app

const PORT = process.env.PORT || 3000
app.listen(PORT, () => console.log(`running on port ${PORT}`))
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
    if (config.users.includes(ctx.request.headers.mail)) {
    //if (config.users.includes("hannamari.h.heiniluoma@jyu.fi")) {
        ctx.status = 200
        await next()
    } else {
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

//TODO: voiko yhdistää yllä olevan getTitlesin kanssa?
const getTitle = (record) => {
    const titleFieldRegex = /<datafield tag="245"((.|\s)*?)<\/datafield>/g
    const beforeTitleRegex = /<datafield tag="245"((.|\s)*?)<subfield code="a">/g
    const afterTitleRegex = /<\/subfield>((.|\s)*?)$/g
    const titlefield = record.match(titleFieldRegex)
    const title = titlefield[0].replace(beforeTitleRegex, "").replace(afterTitleRegex, "")
    return title
}

const splitOutput = () => {
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
        recordNumbers.push(parseInt(errorNumber))
    }
    const titles = getTitles()
    let erroneousRecords = []
    for (let i = 0; i < recordNumbers.length; i++) {
        erroneousRecords.push({
            titleNumber: recordNumbers[i],
            title: titles[recordNumbers[i]-1]
        })
    }
    return erroneousRecords
}

//-----------routes--------------------------------

router.get("/auth", async (ctx, next) => {
    await next()
})

router.post('/convert', upload.single('file'), async (ctx) => {

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

        let errors = true

        if (!result.includes("ERROR")/* && !result.includes("WARNING")*/) {
            result = result.split("100% ")[1]
            errors = false
        }

        let errorRecords = []
        if (errors) {
            try {
                errorRecords = getRecordsWithErrors(result)
            } catch (TypeError) {
                logger.error({
                    message: "TypeError, ERROR-viesti jota palvelin ei osaa käsitellä.",
                    file: ctx.request.body.filename,
                    errorMessage: result
                })
            }
        }

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

    const titles = ctx.request.body.titles

    let records = []
    try {
        records = splitOutput()
    } catch (error) {
        ctx.status = 500
        return ctx.body = {
            error: "Konvertoitua marc-tiedostoa ei löydy."
        }
    }

    let biblios = []
    let kohaError = ""

    // etsitään postattavat tietueet nimekkeen perusteella ja postataan ne Kohaan
    let titlesFound = 0
    for (const record of records) {
        let title = ""
            try {
                title = getTitle(record)
            } catch {
                continue 
            }  
        if (titles.includes(title)) {
            titlesFound++
            await axios({
                method: "POST",
                data: record,
                url: "https://app1.jyu.koha.csc.fi/api/v1/contrib/natlibfi/biblios",
                headers: {
                    'Content-Type': 'text/xml',
                    'Authorization': `Basic ${process.env.BASIC}`
                }
            }).then((response) => {
                biblios.push({title: title, url: `https://app1.jyu.koha.csc.fi/cgi-bin/koha/catalogue/detail.pl?biblionumber=${response.data.biblio_id}`})
            }).catch(error => {
                kohaError = `Tietueen ${title} tallentaminen Kohaan epäonnistui`
            })
        } if (kohaError || titlesFound >= titles.length) {  //jos kaikki titlet on postattu, voidaan for-looppi lopettaa
            break
        }
    } 
    if (kohaError) {
        logger.error({
            käyttäjä: ctx.request.headers.mail,
            kohaError: kohaError
        })
        logger.info({
            message: "Tallennettu Kohaan",
            user: ctx.request.headers.mail,
            biblios: biblios
        })
        // tässä tapauksessa kuitenkin onnistuneet tietueet UI:hin (ja lokiin)
        ctx.status = 500
        return ctx.body = {
            error: kohaError,
            biblios: biblios
        }
    }
    logger.info({
        message: "Tallennettu Kohaan",
        user: ctx.request.headers.mail,
        biblios: biblios
    })
    ctx.body = {
        biblios: biblios
    }
})


app.use(router.routes()).use(router.allowedMethods())

module.exports = app

const PORT = process.env.PORT || 3000
app.listen(PORT, () => console.log(`running on port ${PORT}`))
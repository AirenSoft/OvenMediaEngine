const base64url = require('base64url') // you will probably have to add this package `npm i base64url`
const crypto = require('crypto')

const help = `Usage: 
    ${process.argv[0].split('/').slice(-1)} ${process.argv[1].split('/').slice(-1)} [HMAC_KEY] [BASE_URL] [SIGNATURE_QUERY_KEY_NAME] [POLICY_QUERY_KEY_NAME] [POLICY]
Example:
    ${process.argv[0].split('/').slice(-1)} ${process.argv[1].split('/').slice(-1)} ome_is_the_best ws://host:3333/app/stream signature policy '{\"url_expire\":1604377520178}'
Example Output:
    ws://host:3333/app/stream?policy=e1widXJsX2V4cGlyZVwiOjE2MDQzNzc1MjAxNzh9&signature=rXfU1z1ynBTfK-q6_HM_I9fPzRs
`

const args = process.argv.slice(2)
if (args.length != 5){
  console.log(help)
  process.exit(0)
}

const HMAC_KEY                  = args[0]
const BASE_URL                  = args[1]
const SIGNATURE_QUERY_KEY_NAME  = args[2]
const POLICY_QUERY_KEY_NAME     = args[3]
const POLICY                    = args[4]

let baseUrl = BASE_URL
// console.log('baseUrl:\t', baseUrl)

let policy = POLICY
// console.log('policy:\t\t',policy)

let policyBase64 = base64url(Buffer.from(policy,'utf8'))
// console.log('policyBase64:\t',policyBase64)

let policyUrl = baseUrl + '?' + POLICY_QUERY_KEY_NAME + '=' + policyBase64
// console.log('policyUrl:\t', policyUrl)

let signature = base64url(crypto.createHmac('sha1', HMAC_KEY).update(policyUrl).digest())
// console.log('signature:\t', signature)

let signedUrl = policyUrl + '&' + SIGNATURE_QUERY_KEY_NAME + '=' + signature
// console.log('signedUrl:\t', signedUrl)

console.log(signedUrl)

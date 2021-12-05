require('cross-fetch/polyfill')
const ApolloClient = require('apollo-boost').ApolloClient;
const InMemoryCache = require('apollo-cache-inmemory').InMemoryCache;
const createHttpLink = require('apollo-link-http').createHttpLink;
const gql = require('graphql-tag')

const defaultOptions = {
  watchQuery: {
    fetchPolicy: 'no-cache',
    errorPolicy: 'ignore',
  },
  query: {
    fetchPolicy: 'no-cache',
    errorPolicy: 'ignore',
  },
}

let client

const widthFor12ptFont = [
  0,
  10,
  10,
  10,
  10,
  10,
  10,
  10,
  10,
  8,
  10,
  10,
  10,
  0,
  10,
  10,
  10,
  11,
  11,
  9,
  11,
  10,
  10,
  10,
  10,
  10,
  10,
  10,
  10,
  10,
  10,
  10,
  4,
  6,
  7,
  10,
  7,
  11,
  10,
  3,
  5,
  5,
  7,
  7,
  4,
  7,
  4,
  7,
  8,
  8,
  8,
  8,
  8,
  8,
  8,
  8,
  8,
  8,
  4,
  4,
  6,
  8,
  6,
  8,
  11,
  8,
  8,
  8,
  8,
  7,
  7,
  8,
  8,
  6,
  7,
  9,
  7,
  12,
  9,
  8,
  8,
  8,
  8,
  7,
  6,
  8,
  8,
  12,
  8,
  8,
  8,
  5,
  7,
  5,
  8,
  8,
  6,
  8,
  8,
  7,
  8,
  8,
  6,
  8,
  8,
  4,
  6,
  8,
  4,
  12,
  8,
  8,
  8,
  8,
  6,
  7,
  6,
  8,
  8,
  12,
  8,
  8,
  8,
  5,
  5,
  5,
  8,
  8
]


// this is tied to Sample.c's message window max width
const MAX_WIDTH = 304
const SPACE_WIDTH = widthFor12ptFont[32]

const getNextWordLength = (word) => {

  let currentWidth = 0

  for (const char of word.split(``)) {

    let currentCharWidth = widthFor12ptFont[char.charCodeAt()]

    if (isNaN(currentCharWidth)) {

    currentCharWidth = 1;
    }

    currentWidth += currentCharWidth
  }

return currentWidth
}

const shortenText = (text) => {

  let outputText = ``
  let currentWidth = 0

  for (const word of text.split(` `)) {

    let currentWordWidth = getNextWordLength(word)

    if (currentWidth + currentWordWidth + SPACE_WIDTH > MAX_WIDTH) {

    outputText = `${outputText}ENDLASTMESSAGE`
    currentWidth = 0

    // okay, but what if the word itself is greater than max width?
    if (currentWordWidth > MAX_WIDTH) {

      let splitWordWidth = 0

      for (const char of word.split(``)) {

        let currentCharWidth = widthFor12ptFont[char.charCodeAt()]

        if (isNaN(currentCharWidth)) {
      
          currentCharWidth = 1;
        }

        if (splitWordWidth + currentCharWidth > MAX_WIDTH) {

          outputText = `${outputText}ENDLASTMESSAGE`
          splitWordWidth = 0
        }

        splitWordWidth += currentCharWidth
        outputText = `${outputText}${char}`
        }

        currentWidth += splitWordWidth
        
        continue
      }
    }

    currentWidth += currentWordWidth + SPACE_WIDTH
    outputText = `${outputText} ${word}`
  }

  return outputText
}

const MAX_ROWS = 16

const splitMessages = (messages) => {

  let firstMessage = true

  if (!messages) {

    return `no messages ENDLASTMESSAGE`
  }

  for (const message of messages) {

    if (firstMessage) {

      let tempMessageOutput = `${message.chatter}: ${message.text}`

      tempMessageOutput = shortenText(tempMessageOutput)
      messageOutput = tempMessageOutput
    } else {

      let tempMessageOutput = `${message.chatter}: ${message.text}`

      tempMessageOutput = shortenText(tempMessageOutput)
      messageOutput = `${messageOutput}ENDLASTMESSAGE${tempMessageOutput}`
    }

    firstMessage = false
  }


  if (messageOutput.split(`ENDLASTMESSAGE`).length > MAX_ROWS) {

    messageOutput = messageOutput.split(`ENDLASTMESSAGE`)

    let newMessageOutput = []

    for (let i = messageOutput.length; i > messageOutput.length - MAX_ROWS; i--) {

      newMessageOutput.unshift(messageOutput[i])
    }

    messageOutput = newMessageOutput.join(`ENDLASTMESSAGE`)
  } 

  lastMessageOutput = messageOutput

  return messageOutput
}

let lastMessageOutput

class iMessageClient {

  async getMessages (chatId, page) {

    console.log(`get messages for chat ID: ${chatId}`)

    let result

    try {
    
      result = await client.query({
        query: gql`query getMessages {
            getMessages(chatId: "${chatId}", page: "${page}") {
                chatter
                text
            }
        }`
      })
    } catch (error) {

      console.log(`error with apollo query`)
      console.log(error)

      result = {
        data: {
        }
      }
    }

    let messages = result.data.getMessages

    return splitMessages(messages)
  }

  async hasNewMessagesInChat (chatId) {

    let currentLastMessageOutput = `${lastMessageOutput}`
    let messageOutput = await this.getMessages(chatId, 0)

    return (currentLastMessageOutput !== messageOutput).toString()
  }

  async hasNewMessages (chatId) {

    let currentLastMessageOutput = `${lastMessageOutput}`
    let messageOutput = await this.getChats(chatId, 0)

    return (currentLastMessageOutput !== messageOutput).toString()
  }

  async sendMessage (chatId, message) {

    console.log(`send message`)
    console.log(chatId)
    console.log(message)

    let result

    try {
    
      result = await client.query({
        query: gql`query sendMessage {
            sendMessage(chatId: "${chatId}", message: "${message}") {
                chatter
                text
            }
        }`
      })
    } catch (error) {

      console.log(`error with apollo query`)
      console.log(error)

      result = {
        data: {
        }
      }
    }

    let messages = result.data.sendMessage

    return splitMessages(messages)
  }

  async getChats () {

    let result

    try {
    
      result = await client.query({
        query: gql`query getChats {
            getChats {
                name
                friendlyName
            }
        }`
      })
    } catch (error) {

      console.log(`error with apollo query`)
      console.log(error)

      result = {
        data: {
        }
      }
    }

    let chats = result.data.getChats
    let friendlyNameStrings = ``

    if (!chats) {

      return ``
    }

    for (let chat of chats) {

      friendlyNameStrings = `${friendlyNameStrings},${chat.friendlyName.replace(/,/g, '')}`
    }

    friendlyNameStrings = friendlyNameStrings.substring(1, friendlyNameStrings.length)

    console.log(`chats`)
    console.log(friendlyNameStrings)

    return friendlyNameStrings
  }

  setIPAddress (IPAddress) {

    console.log(`instantiate apolloclient with uri ${IPAddress}:4000/`)

    try {

      client = new ApolloClient({
        uri: `${IPAddress}:4000/`,
        cache: new InMemoryCache(),
        link: new createHttpLink({
          uri: `${IPAddress}:4000/`
        }),
        defaultOptions
      });
    } catch (err) {
      console.log(`error instantiating the ApolloClient`)
      console.log(err)

      return `failure`
    }

    return `success`
  }
}

module.exports = iMessageClient


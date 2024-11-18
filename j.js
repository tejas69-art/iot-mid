const express = require('express');
const bodyParser = require('body-parser');
const crypto = require('crypto');
const axios = require('axios');
require('dotenv').config();
const app = express();
const port = 3000;


const WEBHOOK_SECRET = process.env.WEBHOOK_SECRET;
const FIREBASE_DB_URL = process.env.FIREBASE_DB_URL;

// Middleware to parse JSON bodies and capture raw body for signature verification
app.use(bodyParser.json({
  verify: (req, res, buf) => {
    req.rawBody = buf.toString();
  }
}));

// Webhook endpoint to receive Razorpay webhooks
app.post('/webhook', (req, res) => {
  const signature = req.headers['x-razorpay-signature'];

  if (!signature) {
    console.log('No signature provided');
    return res.status(401).send('No signature provided');
  }

  try {
    // Validate the webhook signature
    const expectedSignature = crypto
      .createHmac('sha256', WEBHOOK_SECRET)
      .update(req.rawBody)
      .digest('hex');

    if (signature !== expectedSignature) {
      console.log('Invalid signature');
      return res.status(401).send('Invalid signature');
    }

    console.log('Received webhook payload:', req.body);

    // Extract the payment amount and device ID from the webhook payload
    const paymentAmount = req.body.payload.payment.entity.amount;
    const formattedAmount = paymentAmount / 100;
    const deviceID = req.body.payload.payment.entity.notes.device; // Device ID from Razorpay notes

    if (!deviceID) {
      console.log('No device ID found in webhook payload');
      return res.status(400).send('No device ID found');
    }

    console.log(`Payment Amount Received: Rs. ${formattedAmount} for device ${deviceID}`);

    // Send the payment amount to the specific device path in Firebase Realtime Database
    sendToFirebase(formattedAmount, deviceID);

    res.status(200).send('Webhook received and processed successfully');
  } catch (error) {
    console.error('Error processing webhook:', error);
    res.status(500).send('Internal server error');
  }
});

function sendToFirebase(amount, deviceID) {
  const firebaseUrl = `${FIREBASE_DB_URL}/devices/${deviceID}/payment.json`;

  axios.put(firebaseUrl, {
    timestamp: Date.now(),
    value: amount
  }, {
    headers: {
      'Content-Type': 'application/json'
    }
  })
    .then(response => {
      console.log(`Data sent to Firebase for device ${deviceID} successfully:`, response.data);
    })
    .catch(error => {
      console.error(`Error sending data to Firebase for device ${deviceID}:`, error.response ? error.response.data : error.message);
    });
}

app.listen(port, () => {
  console.log(`Webhook middleware listening at http://localhost:${port}`);
});

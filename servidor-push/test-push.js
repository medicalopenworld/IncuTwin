const { Expo } = require('expo-server-sdk');

const expo = new Expo();
const token = 'ExponentPushToken[EOzRSoLRZXa-LsOX5L84F_]';

(async () => {
  if (!Expo.isExpoPushToken(token)) {
    console.error('Token inválido');
    process.exit(1);
  }

  try {
    const [ticket] = await expo.sendPushNotificationsAsync([{
      to: token,
      title: '🔔 Test notificación',
      body: 'Si ves esto, las push notifications funcionan.',
      sound: 'default',
      priority: 'high',
    }]);

    console.log('Ticket:', JSON.stringify(ticket, null, 2));

    if (ticket.status === 'ok') {
      console.log('✅ Notificación enviada a Expo. Comprueba el móvil.');
    } else {
      console.error('❌ Error:', ticket.message);
    }
  } catch (e) {
    console.error('Error:', e.message);
  }
})();

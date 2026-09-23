// Convierte el envelope del evento en un registro de telemetría auditable en la propia IncuNest.
// metadata.ts ya es el ts del evento (fijado por el script anterior).
return { msg: { itw_event: JSON.stringify(msg), itw_event_type: msg.event, itw_event_seq: msg.seq }, metadata: metadata, msgType: 'POST_TELEMETRY_REQUEST' };

# Wisotronic RS485 Protocol Notes

Findings from reverse-engineering the Wisotronic bus (`warema_raw_data` UART dump,
`esphome/warema.yaml`, ESPHome custom component `my_components/wisotronic`).

## Physical layer (as configured in `warema.yaml`)

- RS485 half-duplex, bus powered at 24 V, JY(St) 2x2x0.8 mm², 120 Ω termination.
- Baud rate 19200, 8N1 (assumed).
- Protocol is bus-level broadcast/loopback: frames flow continuously, no request/response
  timing handshake.

## Frame format (confirmed)

Each frame:

```
[0]     0x23                    magic start byte
[1]     class                  0x00 (controller / temp) or 0xfd (station / covers)
[2]     direction/addr         0x00 / 0xfd (polls), 0xff (data frames)   [approx]
[3]     0x1c (data) / 0x10 (poll)
[4]     0x10 (data) / 0x01 (poll)
[5]     0x01
[6]     0x00
[7]     dataset id              0x01 / 0x02 / 0x03                            [data]
[8]     L                        payload length                                [data]
[9..9+L-1] TLV payload
[9+L]   checksum                additive sum of all previous bytes mod 256
```

- Data frame total length = `L + 10`. Validated: len 22 -> L=12, len 46 -> L=36, len 58 -> L=48.
- Poll frame (11 bytes, class 0x00): `23 00 <00|fd> 10 01 <01|02|03> 00 00 00 00 CS`,
  where byte 5 is the requested dataset (1 -> 22 B, 2 -> 46 B, 3 -> 58 B).
- Checksum = sum of all bytes before the last byte, modulo 256 (uint8 wraparound). Validated
  against every frame (10,959 of 10,960 checksum-valid frames passed).
- Frames are back-to-back; the next frame's leading `0x23` terminates the previous one.

## Payload structure

Payload is a TLV chain: `[tag][length][value bytes...]`, with the tag/length/value chain
consuming exactly `L` bytes. Validated over all 10,970 data frames: 0 parse failures, 0
length mismatches.

| Frame | Class | Tags | Notes |
|---|---|---|---|
| 11 B | 0x00 | - | poll / heartbeat (dataset select) |
| 22 B | 0x00 | 0x00,0x01,0x02,0x03 (len 1 each) | sensor frame; tag 0x00 = control unit temp raw |
| 22 B | 0xfd | 0x12,0x13,0x14,0x15 (len 1 each) | cover/status frame; blind channels |
| 46 B | 0xfd | 0x00 (len 6), 0x01 (len 6), 0x02 (len 6), 0x03 (len 1), 0x11 (len 7) | covers + wind |
| 58 B | 0xfd | 0x04..0x16 | full weather payload |

## Field mapping (as decoded from tag values + scaling)

- control unit / indoor temperature: raw = value*2 - 25 (°C)
- outdoor temperature: raw = value*2 - 25 (°C)
- humidity: raw = value*2 (percent)
- photo brightness: raw = value*2*1000 (lx)
- twilight: value = lx*2
- wind speed: raw = value*5.0 (m/s)
- cover channel blocks (46 B frame, tag 0x00/0x01/0x02, len 6): block `28 08 <p> <t> <p> <t>`;
  e.g. tag 0x02 block was `28 08 00 b0 00 b0`.

## Uncovered / missing data discovered in the dump

The pre-existing parser only handled:

1. all 11 B frames -> not handled at all (poll frames)
2. 22 B frames with class 0x00 -> only the sensor/temperature frame; 22 B frames with class
   0xfd (blind status, tags 0x12-0x15) were silently ignored
3. TLV fields inside handled frames that were never decoded:
   - tag 0x11 (len 7) in the 46 B frame (sequence-like counter, bytes 2-6 constant)
   - tags 0x0e / 0x0f / 0x10 (len 3 each, constant `07 00 80`) in the 58 B frame
   - tag 0x16 (len 1, value 0x07) in the 58 B frame
   - tags 0x01/0x02/0x03 in the 22 B sensor frame (currently zero)

## Parser implications

- Replace magic-byte resynchronization (`0x23` + last-two-byte check) with a length-field
  driven parser (use byte 8; total = byte8 + 10) + additive checksum validation.
- Parse payloads as TLV chains (tag/length/value) rather than fixed offsets; validate that the
  TLV chain consumes exactly the declared payload length.
- Route frames by (class, dataset) and map responses back to the requesting poll.

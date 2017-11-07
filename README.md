# vsam
This NodeJS module enables you to read VSAM files on z/OS

## Installation

<!--
This is a [Node.js](https://nodejs.org/en/) module available through the
[npm registry](https://www.npmjs.com/).
-->

Before installing, [download and install Node.js](https://developer.ibm.com/node/sdk/ztp/).
Node.js 6.11.4 or higher is required.

This will soon be available for install on npm.

```javascript
vsam( "//'SAMPLE.TEST.VSAM.KSDS'",
      JSON.parse(fs.readFileSync('test.json')),
      (file) => {
        file.find( "0321", (record, err) => {
          if (err != null)
            console.log("Not found!");
          else {
            assert(record.key, "0321");
            console.log(`Current details: Name(${record.name}), Gender(${record.gender})`);
            record.name = "KEVIN";
            file.update(record, (err) => {
              file.close();
            });
          }
        }
);
```
test.json looks like this:

```json
{
  "key": {
    "type": "string",
    "maxLength": 5
  },
  "name": {
    "type": "string",
    "maxLength": 10
  },
  "gender": {
    "type": "string",
    "maxLength": 10
  }
}
```

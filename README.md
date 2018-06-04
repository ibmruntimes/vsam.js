# vsam
This NodeJS module enables you to read VSAM datasets on z/OS

## Installation

<!--
This is a [Node.js](https://nodejs.org/en/) module available through the
[npm registry](https://www.npmjs.com/).
-->

Before installing, [download and install Node.js](https://developer.ibm.com/node/sdk/ztp/).
Node.js 6.11.4 or higher is required.

## Simple to use

Vsam.js is designed to be a bare bones vsam I/O module.

```js
try {
  dataset = vsam.open("sample.test.vsam.ksds",
                   JSON.parse(fs.readFileSync('schema.json')));
  dataset.find("0321", (record, err) => {
      if (err != null)
        console.log("Not found!");
      else {
        assert(record.key, "0321");
        console.log(`Current details: Name(${record.name}), Gender(${record.gender})`);
        record.name = "KEVIN";
        dataset.update(record, (err) => {
          dataset.close();
        });
      }
    }
);
```
schema.json looks like this:

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

## Table of contents

- [Opening a vsam dataset for I/O](#opening-a-vsam-dataset-for-io)
- [Allocating a vsam dataset for I/O](#allocating-a-vsam-dataset-for-io)
- [Check if vsam dataset exists](#check-if-vsam-dataset-exists)
- [Closing a vsam dataset](#closing-a-vsam-dataset)
- [Reading a record from a vsam dataset](#reading-a-record-from-a-vsam-dataset)
- [Writing a record to a vsam dataset](#writing-a-record-to-a-vsam-dataset)
- [Finding a record in a vsam dataset](#finding-a-record-in-a-vsam-dataset)
- [Updating a record in a vsam dataset](#updating-a-record-in-a-vsam-dataset)
- [Deleting a record from a vsam dataset](#deleting-a-record-from-a-vsam-dataset)
- [Deallocating a vsam dataset](#deallocating-a-vsam-dataset)

---

## ALlocating a vsam dataset for I/O

```js
const vsam = require('vsam');
const fs = require('fs');
var vsamObj = vsam.allocSync("VSAM.DATASET.NAME", JSON.parse(fs.readFileSync('schema.json')));
```

* The first argument is the VSAM dataset name to allocate.
* The second argument is the JSON object derived from the schema file.
* The value returned is a vsam dataset handle. The rest of this readme describes the operations that can be performed on this object.
* Usage notes:
  * If the dataset already exists, this function will throw an exception.
  * On any error, this function with throw an exception.

## Opening a vsam dataset for I/O

```js
const vsam = require('vsam');
const fs = require('fs');
var vsamObj = vsam.openSync("VSAM.DATASET.NAME", JSON.parse(fs.readFileSync('schema.json')));
```

* The first argument is the name of an existing VSAM dataset.
* The second argument is the JSON object derived from the schema file.
* The value returned is a vsam dataset handle. The rest of this readme describes the operations that can be performed on this object.
* Usage notes:
  * On error, this function with throw an exception.

## Check if vsam dataset exists

```js
const vsam = require('vsam');
const fs = require('fs');
vsam.exist("VSAM.DATASET.NAME");
```

* The first argument is the name of an existing VSAM dataset.
* Boolean value is returned indicating whether dataset exists or not.

## Closing a vsam dataset

```js
vsamObj.close((err) => { /* Handle error. */ });
```

* The first argument is a callback function containing an error object if the close operation failed.

## Reading a record from a vsam dataset

```js
vsamObj.read((record, err) => { 
  /* Use the record object. */
});
```

* The first argument is a callback function whose arguments are as follows:
  * The first argument is an object that contains the information from the read record.
  * The second argument will contain an error object in case the read operation failed.
* Usage notes:
  * The read operation retrievs the record under the current cursor and advances the cursor by one record length.

## Writing a record to a vsam dataset

```js
vsamObj.write((record, (err) => { 
  /* Make sure err is null. */
});
```

* The first argument is record object that will be written.
* The second argument is a callback to notify of any error in case the write operation failed.
* Usage notes:
  * The write operation advances the cursor by one record length after the newly written record.
  * The write operation will overwrite any existing record with the same key.

## Finding a record in a vsam dataset

```js
vsamObj.find(recordKey, (record, err) => { 
  /* Use record information. */
});
```

* The first argument is record key (usually a string).
* The second argument is a callback
  * The first argument is a record object retrieved using the key provided.
  * The second argument is an error object in case the operation failed.
* Usage notes:
  * The find operation will place the cursor at the queried record (if found).
  * The record object in the callback will by null if the query failed to retrieve a record.
  
## Updating a record in a vsam dataset

```js
vsamObj.update((recordKey, (err) => { 
   ...
});
```

* The first argument is record key (usually a string).
* The second argument is a callback
  * The first argument is an error object in case the operation failed.
* Usage notes:
  * The update operation will write over the record currently under the cursor.
  
## Deleting a record from a vsam dataset

```js
vsamObj.delete((err) => { /* Handle error. */ });
```

* The first argument is a callback function containing an error object if the delete operation failed.
* Usage notes:
  * The record under the current position of the dataset cursor gets deleted.
  * This will usually be placed inside the callback of a find operation. The find operation places
    the cursor on the desired record and the subsequent delete operation deletes it.

## Deallocating a vsam dataset

```js
vsamObj.dealloc((err) => { /* Handle error. */ });
```

* The first argument is a callback function containing an error object if the deallocation operation failed.

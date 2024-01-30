# vsam
This Node.js module enables an application to read and modify VSAM datasets on z/OS.

## Installation

<!--
This [Node.js](https://nodejs.org/en/) module is available through the [npm registry](https://www.npmjs.com/).
-->

Before installing, [download and install IBM Open Enterprise SDK for Node.js](https://www.ibm.com/docs/en/sdk-nodejs-zos)
16 or higher. vsam.js v3.1.0 or higher is required for Node.js 18 or higher.

## Simple to use

vsam.js is designed to be a bare-bones VSAM I/O module.

```js
  vsamObj = vsam.openSync("sample.test.vsam.ksds",
                          JSON.parse(fs.readFileSync("schema.json")));

  // Find using a string as key:
  vsamObj.find("0321", (record, err) => {
    if (record !== null) {
      assert.equal(record.key, "0321");
      record.name = "KEVIN";
      record.quantity = Buffer.from([0xe5, 0xf6, 0x78, 0x9a]).toString("hex");
      vsamObj.update(record, (err) => {
        if (err !== null)
          console.log("update was successful");
        else
          console.error(err);
        vsamObj.close();
      });
    } else {
      console.error(err);
    }
  });

  // or find using binary data as key (type must be set to "hexadecimal"):
  const keybuf = Buffer.from([0xa1, 0xb2, 0xc3, 0xd4]);
  vsamObj.find(keybuf, keybuf.length, (record, err) => {
    ...

  // or find using a hexadecimal string as key (type must be set to "hexadecimal"):
  vsamObj.find("e5f6789a", (record, err) => {
    ...

  // Starting with vsam.js v3.0.0, find and update record(s) in one call synchronously:
  count = vsamObj.updateSync("f1f2f3", record);

  // or find and delete record(s) in one call:
  count = vsamObj.deleteSync("f1f2f3");

  // or find and update record(s) in one call asynchronously:
  vsamObj.update("f1f2f3", record, (count, err) => { ... });
```
schema.json contains the dataset's field names and their attributes, e.g.:

```json
{
  "key": {
    "type": "hexadecimal",
    "maxLength": 8
  },
  "name": {
    "type": "string",
    "maxLength": 10,
    "minLength": 1
  },
  "quantity": {
    "type": "hexadecimal",
    "maxLength": 4
  }
}
```
See [test/schema.json](https://github.com/ibmruntimes/vsam.js/blob/master/test/schema.json), [test/async.js](https://github.com/ibmruntimes/vsam.js/blob/master/test/async.js) and [test/sync.js](https://github.com/ibmruntimes/vsam.js/blob/master/test/sync.js) for examples on using the functions available in vsam.js.

## Table of contents

- [Supported Data Types](#supported-data-types)
- [Dataset Schema JSON File](#dataset-schema-json-file)
- [Open a VSAM dataset](#open-a-vsam-dataset)
- [Allocate a VSAM dataset](#allocate-a-vsam-dataset)
- [Check if a VSAM dataset exists](#check-if-a-vsam-dataset-exists)
- [Close a VSAM dataset](#close-a-vsam-dataset)
- [Read a record from a VSAM dataset](#read-a-record-from-a-vsam-dataset)
- [Write a record to a VSAM dataset](#write-a-record-to-a-vsam-dataset)
- [Specifying the record key to operate on](#specifying-the-record-key-to-operate-on)
- [Find a record in a VSAM dataset](#find-a-record-in-a-vsam-dataset)
- [Update a record in a VSAM dataset](#update-a-record-in-a-vsam-dataset)
- [Delete a record from a VSAM dataset](#delete-a-record-from-a-vsam-dataset)
- [Deallocate a VSAM dataset](#deallocate-a-vsam-dataset)
- [Find and update or delete record(s) in one asynchronous function call](#find-and-update-or-delete-records-in-one-asynchronous-function-call)
- [Synchronously find, create, read, update and delete functions](#synchronously-find-create-read-update-and-delete-functions)

---

## Supported Data Types

The following data types are supported by vsam.js and are used in the dataset schema JSON file to specify the type of each field of a dataset record:
* string
  * find and write data as a string (character array)
* hexadecimal
  * find and write data as binary data using Node.js Buffer class or a hexadecimal string

## Dataset Schema JSON File

The following are the attributes to specify for each field of a dataset record:
* type
  * specifies the type of data; must be "string" or "kexadecimal"
* maxLength
  * specifies the maximum length of data; must be greater that 0
* minLength (optional, added in v3.0.0)
  * specifies the minimum length of data; for the key field: default is 1 and must be greater than 0; for non-key fields: default is 0

* Usage notes:
  * vsam.js uses the field named "key" as the key field; if no such name is found in the schema, it treats the first field as the key field.
  * Starting with vsam.js v3.0.0, the key field can be anywhere in the record, not just the first.
  * If the data passed to the vsam.js functions violate any of minLength, maxLength, or type (e.g. invalid data for a "hexadecimal" field), an error message is passed to the asynchronous function's callback, or an exception is thrown for the synchronous functions.
  * Prior to v3.0.0, user data that exceeded maxLength was not treated as an error, and was truncated to the maxLength instead.
  * If the data provided for a field has length less than maxLength, vsam.js sets all remaining bytes in the record's field to binary 0, for both "string" and "hexadecimal" field types.

## Allocate a VSAM dataset

```js
const vsam = require("vsam");
const fs = require("fs");
var vsamObj = vsam.allocSync("VSAM.DATASET.NAME", JSON.parse(fs.readFileSync("schema.json")));
```

* The first argument is the dataset name to allocate.
* The second argument is the JSON object derived from the schema file.
* The value returned is a dataset object, and is used when calling any of the functions that operate on this dataset.
* Usage notes:
  * If the dataset already exists, or on error, this function will throw an exception.

## Open a VSAM dataset

```js
const vsam = require("vsam");
const fs = require("fs");
var vsamObj = vsam.openSync("VSAM.DATASET.NAME", JSON.parse(fs.readFileSync("schema.json")));
```

* The first argument is the name of an existing dataset.
* The second argument is the JSON object derived from the schema file.
* The optional third argument is the "mode" passed by vsam.js to the `fopen(filename, mode)` function; default is "rb+,type=record" if none is specified.
* The value returned is a dataset object, and is used when calling any of the functions that operate on this dataset.
* Usage notes:
  * To open a non-empty dataset in read-only mode, specify "rb,type=record" as the third argument.
  * If the dataset doesn't exist, or on error, this function will throw an exception.

## Check if a VSAM dataset exists

```js
const vsam = require("vsam");
if (vsam.exist("VSAM.DATASET.NAME")) {
  /* The dataset exists. */
}
```

* The first argument is the name of a dataset.
* The value returned is a Boolean `true` or `false` indicating whether the given dataset exists or not.

## Close a VSAM dataset

```js
vsamObj.close();
```

* This function takes no argument. Prior to vsam.js v3.0.0, it was wrongly documented as taking a callback argument.
* Usage notes:
  * This function closes the file stream associated with the dataset.
  * This is a synchronous function, and will throw an exception on error (including close() on a dataset that has already been closed).

## Read a record from a dataset

```js
vsamObj.read((record, err) => {
  if (err !== null) {
    /* an error occurred */
  } else if (record !== null) {
    /* record at the current cursor was read successfully */
  } else {
    /* reached end-of-file */
  }
});
```

* The first argument is a callback whose arguments will be set as follows:
  * The first argument is a JavaScript object that contains a member for each field in the dataset record if the read operation was successful, and `null` otherwise.
  * The second argument is a string containing an error message if an error occurred, and `null` otherwise.
* Usage notes:
  * The read operation retrievs the record at the current cursor and advances the cursor by one record length.
  * If no record was found at the current cursor (e.g. cursor is at end-of-file), both `record` and `err` are set to `null`.
  * On success, the value of each field can be accessed as `record.<fieldName>`, example: `record.amount`.

## Write a record to a VSAM dataset

```js
vsamObj.write(record, (err) => {
  if (err !== null) {
    /* an error occurred */
  } else {
    /* new record was written successfully */
  }
});
```

* The first argument is a JSON object that specifies the value of each field in the new record to be written.
* The second argument is a callback whose argument will be set to a string containing an error message if an error occurred, and `null` otherwise.
* Usage notes:
  * The write operation advances the cursor by one record length after the record has been written successfully.
  * The write operation fails if a record with the same key already exists in the dataset. In vsam.js prior to v3.0.0, this behaviour was wrongly documented as "write() will overwrite any existing record with the same key".

## Specifying the record key to operate on

There are two ways to pass the record key to the `find` functions or the overloaded `update` and `delete` functions that accept a `recordKey` argument:

1. \<string\> - JavaScript string
2. <buffer, buffer-length> - JavaScript buffer followed by its length

For example:
```js
// pass recordKey as a string argument:
file.find("f1f2f3f4", (record, err) => { ... });

// or pass recordKey as buffer, buffer-length arguments:
const keybuf = Buffer.from([0xab, 0xb2, 0xc3, 0xd4]);
file.find(keybuf, keybuf.length, (record, err) => { ... });
```
All references to recordKey in this document refer to both ways of passing the record key argument.

## Find a record in a VSAM dataset

```js
vsamObj.find(recordKey, (record, err) => {
  if (err !== null) {
    /* an error occurred, or no record was found */
  } else {
    /* record was found and read successfully */
  }
});
vsamObj.findeq(recordKey, (record, err) => {
  ...
});
vsamObj.findge(recordKey, (record, err) => {
  ...
});
vsamObj.findlast((record, err) => {
  ...
});
vsamObj.findfirst((record, err) => {
  ...
});
```

* The first argument `recordKey` is the key to locate (see [Specifying the record key to operate on](#specifying-the-record-key-to-operate-on)), except for findlast() and findfirst().
* The second argument (or first argument for findlast and findfirst) is a callback whose arguments will be set as follows:
  * The first argument is the record object retrieved for the given key.
  * The second argument is a string containing an error message if an error occurred or if no record was found, and `null` otherwise.
* Usage notes:
  * The find operation will place the cursor at the queried record if found.
  * The record object in the callback will by `null` if the query failed to retrieve a record, including on error or if no record was found.
  * For a full key search, ensure that the length of `recordKey` is `maxLength` as defined in the dataset's schema, otherwise a generic key search (a partial key match) will be performed.

## Update a record in a VSAM dataset

```js
vsamObj.update(record, (err) => {
  if (err !== null) {
    /* an error occurred */
  } else {
    /* record at the current cursor was updated successfully */
  }
});
```

* The first argument is a JSON object that specifies the value(s) of the field(s) to be updated.
* The second argument is a callback whose argument will be set to a string containing an error message if an error occurred, and `null` otherwise.
* Usage notes:
  * The update operation will write over the record at the current cursor.
  * This function is usually placed inside the callback of a find operation, which places the cursor on the desired record and the subsequent update function call updates it. See the overloaded version of `update` [here](#find-and-update-or-delete-records-in-one-asynchronous-function-call) and `updateSync` [here](#synchronously-find-create-read-update-and-delete-functions) that invoke the find and update operations in a single function call.
  * Starting with vsam.js v3.0.0, only those fields in `record` that are defined will be updated; previously fields that were not defined in `record` were incorrectly updated with the string "undefined" (and the update would fail if the field had a "hexadecimal" type).
  * The update operation will fail if the key was being changed (as VSAM does not allow it).

## Delete a record from a VSAM dataset

```js
vsamObj.delete((err) => {
  if (err !== null) {
    /* an error occurred */
  } else {
    /* record at the current cursor was deleted successfully */
  }
```

* The argument is a callback whose argument will be set to a string containing an error message if an error occurred, and `null` otherwise.
* Usage notes:
  * This function deletes the record at the current cursor.
  * This function is usually placed inside the callback of a find operation, which places the cursor on the desired record and the subsequent delete function call deletes it. See the overloaded version of `delete` [here](#find-and-update-or-delete-records-in-one-asynchronous-function-call) and `deleteSync` [here](#synchronously-find-create-read-update-and-delete-functions) that invoke the find and delete operations in a single function call.

## Deallocate a VSAM dataset

```js
vsamObj.dealloc((err) => {
  if (err !== null) {
    /* an error occurred */
  } else {
    /* dataset was deallocated successfully */
  }
```

* The argument is a callback whose argument will be set to a string containing an error message if an error occurred, and `null` otherwise.
* Usage notes:
  * This function removes the dataset.
  * The dataset must be closed before calling this function, otherwise it will fail.

## Find and update or delete record(s) in one asynchronous function call

###### Added in: v3.0.0

The following functions find the record(s) with the given key and update or delete the record(s) found:

See [Specifying the record key to operate on](#specifying-the-record-key-to-operate-on) for the two ways that the `recordKey` argument can be specified.
```js
update(recordKey, record, (count, err) => {..});
delete(recordKey, (count, err) => {..});
```

* Usage notes:
  * After the operation (update or delete) completes, the `count` argument in the callback will contain the number of records updated or deleted.
  * If no record was found for the given key, `count` will contain the number `0`, and `err` will contain "no record found with the key for [update|delete]".
  * If an error occurred, `count` will contain the number `0`, and `err` will contain an error message.
  * Only those fields in `record` that are defined will be updated in the dataset record(s) found.
  * As indicated above, these overloaded versions can result in updating or deleting more than the first record found, unless the length of the key provided matches all `maxLength` bytes of the key field; if the length of the key provided is less than `maxLength`, subsequent records are read and updated or deleted after each matching update or delete, until the initial part of the key in the current record doesn't match the key provided. See also Usage notes under the `find*` functions.

## Synchronously find, create, read, update and delete functions

###### Added in: v3.0.0

The following synchronous functions accept the same input arguments, if any, and behave the same way in terms of I/O functionality as their respective asynchronous counterparts (having the same name but without `Sync`). They don't accept a callback, and they throw an exception on error, and return a `record` object or a `count` number in place of the callback argument(s) in the asynchronous functions. Please refer to the description of the asynchronous functions above for more details on the following functions.

```js
record = findSync(recordKey);
record = findeqSync(recordKey);
record = findgeSync(recordKey);
record = findfirstSync();
record = findlastSync();

record = readSync();

count = updateSync(record);
count = updateSync(recordKey, record);

count = deleteSync();
count = deleteSync(recordKey);

count = writeSync(record);
```
* Usage notes:
  * If any of the `find*Sync` functions didn't find a record, they return `null` and don't throw an exception in this case.
  * `updateSync(recordKey, record)` and `deleteSync(recordKey)` functions return a count of the number of records updated or deleted.
  * `deleteSync()`, `updateSync(record)` and `writeSync(record)` always returns the number `1` on success.
  * All the Sync functions throw an exception on error, including invalid user arguments or VSAM I/O error.

#!/usr/bin/env node
const vsam = require("./build/Release/vsam.node");
const async = require('async');
const fs = require('fs');

console.log("*************** update.js ****************");

function printUntilEnd(file) {
  var end = false;
  async.whilst(
    // Stop at end of file
    () => { return !end },

    // Read the next record
    (callback) => {
      file.read( (record, err) => {
        if (record == null)
          end = true;
        else {
          console.log(JSON.stringify(record));
        }
        callback(null);
      });
    },

    // Finally close
    (err) => {
      if (err)
        console.log("Error:" + err);
      file.close((err) => {});
    }
  );
}

vsam( "//'BARBOZA.TEST.VSAM.KSDS'", 
      JSON.parse(fs.readFileSync('test.json')),
      (file) => {
        file.find( "00100", (record, err) => {
          record.name = "JOHN";
          file.update(record, (err) => {
            console.log(JSON.stringify(record));
            printUntilEnd(file);
          });
        });
      }
);


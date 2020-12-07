/*
 * Licensed Materials - Property of IBM
 * (C) Copyright IBM Corp. 2017. All Rights Reserved.
 * US Government Users Restricted Rights - Use, duplication or disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
*/

const vsam = require("../build/Release/vsam.js.node");
const async = require('async');
const fs = require('fs');
const expect = require('chai').expect;
const should = require('chai').should;
const assert = require('chai').assert;
const { execSync } = require('child_process');

// Set IBM_VSAM_UID environment variable for customer uid
const uid = process.env.IBM_VSAM_UID || execSync('whoami').slice(0, -1);
const testSet = `${uid}.TEST2.VSAM.KSDS2`;

function readUntilEnd(file, done) {
  var end = false;
  async.whilst(
    // Stop at end of file
    () => { return !end },

    // Read the next record
    (callback) => {
      file.read( (record, err) => {
        if (record == null)
          end = true;
        callback(err);
      });
    },

    // Finally close
    (err) => {
      assert.ifError(err);
      expect(file.close()).to.not.throw;
      done();
    }
  );
}

describe("Key Sequenced Dataset HANGS", function() {
  before(function() {
    if (vsam.exist(testSet)) {
      var file = vsam.openSync(testSet,
          JSON.parse(fs.readFileSync('test/test2.json')));
      expect(file.close()).to.not.throw;
      file.dealloc((err) => {
        assert.ifError(err);
      });
    }
  });
  it("ensure test dataset does not exist", function(done) {
    expect(vsam.exist(testSet)).to.be.false;
    done();
  });

  it("create an empty dataset", function(done) {
    var file = vsam.allocSync(testSet,
                             JSON.parse(fs.readFileSync('test/test2.json')));
    expect(file).not.be.null;
    const keybuf = Buffer.from([0xf1, 0xf2, 0xf3, 0xf4]);
    record = {
      key: keybuf.toString('hex'),
      name: "JOE",
      amount: "999"
    };
//test passes if this is uncommented:
//  file.write(record, (err) => {
//    assert.ifError(err);
      expect(file.close()).to.not.throw;
      done();
//  });
  });

  it("write a record ***find fails without the previous write***", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test2.json')));
    const keybuf = Buffer.from([0xa1, 0xb2, 0xc3, 0xd4]);
    record = {
      key: keybuf.toString('hex'),
      name: "JOHN",
      amount: "1234"
    };
    file.write(record, (err) => {
      assert.ifError(err);

      var key = record.key;
      console.log("TODO: FIND key=<" + key + ">...");
      file.find(key, (record, err) => {
        assert.ifError(err);
        assert.equal(record.name, "JOHN", "created record has correct name");
        expect(file.close()).to.not.throw;
        done();
      });
    });
  });

  it("deallocate a dataset", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test2.json')));
    expect(file.close()).to.not.throw;
    file.dealloc((err) => {
      assert.ifError(err);
      done();
    });
  });
});

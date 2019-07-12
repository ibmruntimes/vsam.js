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

describe("Key Sequenced Dataset #2", function() {
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
                             JSON.parse(fs.readFileSync('test/test2.json')))
    expect(file).not.be.null;
    expect(file.close()).to.not.throw;
    done();
  });

  it("ensure test dataset exists", function(done) {
    expect(vsam.exist(testSet)).to.be.true;
    done();
  });

  it("open and close the existing dataset", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test2.json')))
    expect(file).to.not.be.null;
    expect(file.close()).to.not.throw;
    done();
  });

 it("write new record", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test2.json')))
    record = {
      key: "a1b2c3d4",
      name: "JOHN",
      amount: "1234"
    };
    file.write(record, (err) => {
      assert.ifError(err);
      expect(file.close()).to.not.throw;
      done();
    });
  });

 it("write another new record", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test2.json')))
    record = {
      key: "e5f6789afabcd",
      name: "JIM",
      amount: "9876543210"
    };
    file.write(record, (err) => {
      assert.ifError(err);
      expect(file.close()).to.not.throw;
      done();
    });
  });

  it("read a record and verify properties", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test2.json')))
    file.read( (record, err) => {
      assert.ifError(err);
      expect(record).to.not.be.null;
      expect(record).to.have.property('key');
      expect(record).to.have.property('name');
      expect(record).to.have.property('amount');
      expect(file.close()).to.not.throw;
      done();
    });
  });

  it("find existing record and verify data", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test2.json')))
    file.find("a1b2c3d4", (record, err) => {
      assert.ifError(err);
      assert.equal(record.key, "a1b2c3d4", "1. record has been created");
      assert.equal(record.name, "JOHN", "created record has correct name");
      assert.equal(record.amount, "1234", "created record has correct amount");
      expect(file.close()).to.not.throw;
      done();
    });
    file.findlast((record, err) => {
      assert.ifError(err);
      assert.equal(record.key, "e5f6789afabcd", "2. record has been created");
      assert.equal(record.name, "JIM", "created record has correct name");
      assert.equal(record.amount, "9876543210", "created record has correct amount");
      expect(file.close()).to.not.throw;
      done();
    });
    file.findfirst((record, err) => {
      assert.ifError(err);
      assert.equal(record.key, "a1b2c3d4", "3. record has been created");
      assert.equal(record.name, "JOHN", "created record has correct name");
      assert.equal(record.amount, "1234", "created record has correct amount");
      expect(file.close()).to.not.throw;
      done();
    });
    file.findge("43b2c3d0", (record, err) => {
      assert.ifError(err);
      assert.equal(record.key, "a1b2c3d4", "4. record has been created");
      assert.equal(record.name, "JOHN", "created record has correct name");
      assert.equal(record.amount, "1234", "created record has correct amount");
      expect(file.close()).to.not.throw;
      done();
    });
  });

  it("write new record after read", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test2.json')))
    file.read((record, err) => {
      record.key = "e5f6789afabc"; // same as existing key (for JIM) but without the trailing d
      record.name = "JANE";
      record.amount = "4187832145";
      file.write(record, (err) => {
        assert.ifError(err);
        file.find("e5f6789afabc", (record, err) => {
          assert.ifError(err);
          assert.equal(record.key, "e5f6789afabc", "5. record has been created");
          assert.equal(record.name, "JANE", "5. created record has correct name");
          assert.equal(record.amount, "4187832145", "5. created record has correct amount");
          expect(file.close()).to.not.throw;
          done();
        });
      });
    });
  });

  it("delete existing record", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test2.json')))
    file.find("A1B2c3D4", (record, err) => { // mix uppercase and lowercase hex
      file.delete( (err) => {
        assert.ifError(err);
        file.find("A1B2c3D4", (err) => {
          assert.ifError(err);
          expect(file.close()).to.not.throw;
          done();
        });
      });
    });
  });

  it("reads all records until the end", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test2.json')))
    readUntilEnd(file, done);
  });

  it("open a vsam file with incorrect key length", function(done) {
    expect(() => {
      vsam.openSync(testSet,
                    JSON.parse(fs.readFileSync('test/test-error.json')))
    }).to.throw(/Incorrect key length/);
    done();
  });

  it("return error for non-existent dataset", function(done) {
    expect(() => {
      vsam.openSync("A..B",
                    JSON.parse(fs.readFileSync('test/test2.json')))
    }).to.throw(/Invalid dataset name/);
    done();
  });

  it("update existing record and delete it", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test2.json')))
    file.find("e5f6789afabc", (record, err) => {
      assert.ifError(err);
      record.name = "KEVIN";
      record.amount = "678123";
      file.update(record, (err) => {
        assert.ifError(err);
        file.find("e5f6789afabc", (record, err) => {
          assert.ifError(err);
          assert.equal(record.name, "KEVIN", "name was not updated");
          assert.equal(record.amount, "678123", "amount was not updated");
          file.delete( (err) => {
            assert.ifError(err);
            expect(file.close()).to.not.throw;
            done();
           });
        });
      });
    });
  });

  it("deallocate a dataset", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test2.json')))
    expect(file.close()).to.not.throw;
    file.dealloc((err) => {
      assert.ifError(err);
      done();
    });
  });
});

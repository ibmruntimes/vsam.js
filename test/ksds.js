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

const prefix=require("os").userInfo().username;
const testDsn=prefix + ".TEST.VSAM.KSDS2";
const schema=JSON.parse(fs.readFileSync('test/test.json'));

describe("Key Sequenced Dataset", function() {
  before(function() {
    // Cleanup after failed tests
    if (vsam.exist(testDsn)) {
      var file = vsam.openSync(testDsn,
          JSON.parse(fs.readFileSync('test/test.json')));
      expect(file.close()).to.not.throw;
      file.dealloc((err) => {
        assert.ifError(err);
      });
    }
  });

  it("ensure test dataset does not exist", function(done) {
    expect(vsam.exist(testDsn)).to.be.false;
    done();
  });

  it("create an empty dataset", function(done) {
    var file = vsam.allocSync(testDsn, schema);
    expect(file).not.be.null;
    expect(file.close()).to.not.throw;
    done();
  });

  it("ensure test dataset exists", function(done) {
    expect(vsam.exist(testDsn)).to.be.true;
    done();
  });

  it("open and close the existing dataset", function(done) {
    var file = vsam.openSync(testDsn, schema);
    expect(file).to.not.be.null;
    expect(file.close()).to.not.throw;
    done();
  });

 it("write new record", function(done) {
    var file = vsam.openSync(testDsn, schema);
    record = {
      key: "00126",
      name: "JOHN",
      gender: "MALE"
    };
    file.write(record, (err) => {
      assert.ifError(err);
      expect(file.close()).to.not.throw;
      done();
    });
  });

  it("read a record and verify properties", function(done) {
    var file = vsam.openSync(testDsn, schema);
    file.read( (record, err) => {
      assert.ifError(err);
      expect(record).to.not.be.null;
      expect(record).to.have.property('key');
      expect(record).to.have.property('name');
      expect(record).to.have.property('gender');
      expect(file.close()).to.not.throw;
      done();
    });
  });

  it("find existing record and verify data", function(done) {
    var file = vsam.openSync(testDsn, schema);
    file.find("00126", (record, err) => {
      assert.ifError(err);
      assert.equal(record.key, "00126", "record has been created");
      assert.equal(record.name, "JOHN", "created record has correct name");
      assert.equal(record.gender, "MALE", "created record has correct gender");
      expect(file.close()).to.not.throw;
      done();
    });
  });

  it("write new record after read", function(done) {
    var file = vsam.openSync(testDsn, schema);
    file.read((record, err) => {
      record.key = "00125";
      record.name = "JANE";
      record.gender = "FEMALE";
      file.write(record, (err) => {
        assert.ifError(err);
        file.find("00125", (record, err) => {
          assert.ifError(err);
          assert.equal(record.key, "00125", "record has been created");
          assert.equal(record.name, "JANE", "created record has correct name");
          assert.equal(record.gender, "FEMALE", "created record has correct gender");
          expect(file.close()).to.not.throw;
          done();
        });
      });
    });
  });

  it("delete existing record", function(done) {
    var file = vsam.openSync(testDsn, schema);
    file.find("00126", (record, err) => {
      file.delete( (err) => {
        assert.ifError(err);
        file.find("00126", (err) => {
          assert.ifError(err);
          expect(file.close()).to.not.throw;
          done();
        });
      });
    });
  });

  it("reads all records until the end", function(done) {
    var file = vsam.openSync(testDsn, schema);
    readUntilEnd(file, done);
  });

  it("open a vsam file with incorrect key length", function(done) {
    expect(() => {
      vsam.openSync(testDsn,
                    JSON.parse(fs.readFileSync('test/test-error.json')))
    }).to.throw(/Incorrect key length/);
    done();
  });

  it("return error for non-existent dataset", function(done) {
    expect(() => {
      vsam.openSync("A..B", schema);
    }).to.throw(/Invalid dataset name/);
    done();
  });

  it("update existing record and delete it", function(done) {
    var file = vsam.openSync(testDsn, schema);
    file.find("00125", (record, err) => {
      assert.ifError(err);
      record.name = "KEVIN";
      record.gender = "MALE";
      file.update(record, (err) => {
        assert.ifError(err);
        file.find("00125", (record, err) => {
          assert.ifError(err);
          assert.equal(record.name, "KEVIN", "name has been updated");
          assert.equal(record.gender, "MALE", "gender has been updated");
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
    var file = vsam.openSync(testDsn, schema);
    expect(file.close()).to.not.throw;
    file.dealloc((err) => {
      assert.ifError(err);
      done();
    });
  });
});

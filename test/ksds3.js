/*
 * Licensed Materials - Property of IBM
 * (C) Copyright IBM Corp. 2017. All Rights Reserved.
 * US Government Users Restricted Rights - Use, duplication or disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
*/

/* This is similar to test/ksds2.js, except here only *Sync APIs are used */

const vsam = require("../build/Release/vsam.js.node");
const fs = require('fs');
const expect = require('chai').expect;
const should = require('chai').should;
const assert = require('chai').assert;
const { execSync } = require('child_process');

// Set IBM_VSAM_UID environment variable for customer uid
const uid = process.env.IBM_VSAM_UID || execSync('whoami').slice(0, -1);
const testSet = `${uid}.TEST3.VSAM.KSDS3`;

function readUntilEnd(file, done) {
  while (true) {
    record = file.readSync();
    if (record == null)
      break;
  }
  expect(file.close()).to.not.throw;
  done();
}

describe("Key Sequenced Dataset #3 (sync)", function() {
  before(function() {
    if (vsam.exist(testSet)) {
      var file = vsam.openSync(testSet,
          JSON.parse(fs.readFileSync('test/test3.json')));
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

  it("create an empty dataset, test write and delete on object returned by allocSync", function(done) {
    var file = vsam.allocSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    expect(file).not.be.null;
    const keybuf = Buffer.from([0x91, 0xb2, 0xc3, 0xd4]);
    var record = {
      key: keybuf.toString('hex'),
      name: "JOHN",
      amount: "1234"
    };
    var count = file.writeSync(record);
    assert.equal(count, 1);
    record = file.findSync(keybuf, keybuf.length);
    assert.equal(record.key, "91b2c3d4");
    assert.equal(record.name, "JOHN");
    assert.equal(record.amount, "1234");

    count = file.deleteSync(record.key);
    assert.equal(count, 1);
    expect(file.close()).to.not.throw;
    done();
  });

  it("ensure test dataset exists", function(done) {
    expect(vsam.exist(testSet)).to.be.true;
    done();
  });

  it("verify error from I/O calls on a closed dataset", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    expect(file.close()).to.not.throw;
    var record = { key: "1234", name: "diana", amount: "1234" }
    const keybuf = Buffer.from([0x01, 0x02, 0x03, 0x04]);
    expect(() => { file.readSync();}).to.throw(/readSync error: VSAM dataset is not open./);
    expect(() => { file.findSync("1234");}).to.throw(/findSync error: VSAM dataset is not open./);
    expect(() => { file.findeqSync("1234");}).to.throw(/findSync error: VSAM dataset is not open./);
    expect(() => { file.findgeSync("1234");}).to.throw(/findgeSync error: VSAM dataset is not open./);
    expect(() => { file.findfirstSync();}).to.throw(/findfirstSync error: VSAM dataset is not open./);
    expect(() => { file.findlastSync();}).to.throw(/findlastSync error: VSAM dataset is not open./);
    expect(() => { file.updateSync(record);}).to.throw(/updateSync error: VSAM dataset is not open./);
    expect(() => { file.updateSync("f1f2",record);}).to.throw(/updateSync error: VSAM dataset is not open./);
    expect(() => { file.updateSync(keybuf,keybuf.length,record);}).to.throw(/updateSync error: VSAM dataset is not open./);
    expect(() => { file.deleteSync();}).to.throw(/deleteSync error: VSAM dataset is not open./);
    expect(() => { file.deleteSync("f1f2");}).to.throw(/deleteSync error: VSAM dataset is not open./);
    expect(() => { file.deleteSync(keybuf,keybuf.length);}).to.throw(/deleteSync error: VSAM dataset is not open./);
    expect(() => { file.writeSync(record);}).to.throw(/writeSync error: VSAM dataset is not open./);
    expect(() => { file.close();}).to.throw(/close error: VSAM dataset is not open./);
    done();
  });

  it("verify error from zero-length key buffer", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    const keybuf = Buffer.from([]);
    expect(() => {
      var record = file.findSync(keybuf, keybuf.length);
    }).to.throw(/findSync error: length of 'key' must be 1 or more./);
    expect(file.close()).to.not.throw;
    done();
  });

  it("verify error from zero-length key string", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    expect(() => {
      var record = file.findSync("");
    }).to.throw(/findSync error: length of 'key' must be 1 or more./);
    expect(file.close()).to.not.throw;
    done();
  });

  it("verify error from hexadecimal key buffer exceeding max length", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    const keybuf = Buffer.from([0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0xa7, 0xb8, 0x00]);
    expect(() => {
      var record = file.findSync(keybuf, keybuf.length);
    }).to.throw(/findSync error: length 9 of 'key' exceeds schema's length 8./);
    expect(file.close()).to.not.throw;
    done();
  });

  it("verify error from hexadecimal key string exceeding max length", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    expect(() => {
      var record = file.findSync("0xa1b2c3d4e5f6a7a800");
    }).to.throw(/findSync error: number of hex digits 9 for 'key' exceed schema's length 8, found <0xa1b2c3d4e5f6a7a800>./);
    expect(file.close()).to.not.throw;
    done();
  });

  it("verify error from hexadecimal key string containing non-hex", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    expect(() => {
      var record = file.findSync("0xa1g2c3d4e5f6a7a800");
    }).to.throw(/findSync error: hex string for 'key' must contain only hex digits 0-9 and a-f or A-F, with an optional 0x prefix, found <0xa1g2c3d4e5f6a7a800>./);
    expect(file.close()).to.not.throw;
    done();
  });

  it("verify error from hexadecimal key string prefixed with invalid 0y", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    expect(() => {
      var record = file.findSync("0ya1b2c3d4e5f6a7a800");
    }).to.throw(/findSync error: hex string for 'key' must contain only hex digits 0-9 and a-f or A-F, with an optional 0x prefix, found <0ya1b2c3d4e5f6a7a800>./);
    expect(file.close()).to.not.throw;
    done();
  });

  it("verify error from write record with hexadecimal key string exceeding max length", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    const keybuf = Buffer.from([0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0xa7, 0xb8, 0x00]);
    record = {
      key: keybuf.toString('hex'),
      name: "JOHN",
      amount: "1234"
    };
    expect(() => {
      var count = file.writeSync(record);
    }).to.throw(/writeSync error: number of hex digits 9 for 'key' exceed schema's length 8, found <a1b2c3d4e5f6a7b800>./);
    expect(file.close()).to.not.throw;
    done();
  });

  it("verify error from write record with a field exceeding max length", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    const keybuf = Buffer.from([0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0xa7, 0xb8]);
    record = {
      key: keybuf.toString('hex'),
      name: "exceed by 1",
      amount: "1234"
    };
    expect(() => {
      file.writeSync(record);
    }).to.throw(/writeSync error: length 11 of 'name' exceeds schema's length 10./);
    expect(file.close()).to.not.throw;
    done();
  });

  it("write record on empty dataset and find it, write same record again and verify error", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    const keybuf = Buffer.from([0xa1, 0xb2, 0xc3, 0xd4]);
    var record = {
      key: keybuf.toString('hex'),
      name: "JOHN",
      amount: "1234"
    };
    var count = file.writeSync(record);
    assert.equal(count, 1);
    record = file.findSync(keybuf, keybuf.length);
    assert.equal(record.key, "a1b2c3d4");
    assert.equal(record.name, "JOHN");
    assert.equal(record.amount, "1234");

    expect(() => {
      var count = file.writeSync(record);
    }).to.throw(/write error: an attempt was made to store a record with a duplicate key: EDC5065I A write system error was detected. \(R15=8, errno2=0xc0500091\)./);
    expect(file.close()).to.not.throw;
    done();
  });

  it("write another new record, provide key as Buffer", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
// not supported yet:
//    key: Buffer.from([0xe5, 0xf6, 0x78, 0x9a, 0xfa, 0xbc, 0xd]),
//    key: Buffer.from([0xe5, 0xf6, 0x78, 0x9a, 0xfa, 0xbc, 0xd]).toString('hex'),
    record = {
      // trailing 00 will be truncated when read
      key: "e5f6789afabcd000",
      name: "JIM",
      amount: "9876543210"
    };
    var count = file.writeSync(record);
    assert.equal(count, 1);
    expect(file.close()).to.not.throw;
    done();
  });

  it("verify error from updating a record that results in a duplcate key", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    expect(() => {
      var record = file.findSync("e5f6789afabcd000");
      expect(record).not.be.null;
      record.key = "a1b2c3d4";
      expect(() => {
        var count = file.updateSync(record);
      }).to.throw(/update error: an attempt was made to store a record with a duplicate key: EDC5065I A write system error was detected. (R15=8, errno2=0xc0500090)./);
    }).to.not.throw;
    expect(file.close()).to.not.throw;
    done();
  });

  it("verify update with value length less than minLength specified", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    expect(() => {
      var record = file.findSync("e5f6789afabcd000");
      expect(record).not.be.null;
      record.name = "";
      expect(() => {
        var count = file.updateSync(record);
      }).to.throw(/updateSync error: length of 'name' must be 1 or more./);
    }).to.not.throw;
    expect(file.close()).to.not.throw;
    done();
  });

  it("read a record and verify properties", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    var record = file.readSync();
    expect(record).to.not.be.null;
    expect(record).to.have.property('key');
    expect(record).to.have.property('name');
    expect(record).to.have.property('amount');
    expect(file.close()).to.not.throw;
    done();
  });

  it("verify find, findlast, findfirst, findge followed by a bunch of write/update/find/delete", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    var keybuf = Buffer.from([0xa1, 0xb2, 0xc3, 0xd4]);
    var record = file.findSync(keybuf, keybuf.length);
    expect(record).to.not.be.null;
    assert.equal(record.key, "a1b2c3d4", "record has been created");
    assert.equal(record.name, "JOHN", "created record has correct name");
    assert.equal(record.amount, "1234", "created record has correct amount");

    record = file.findlastSync();
    expect(record).to.not.be.null;
    assert.equal(record.key, "e5f6789afabcd0", "record has been created (trailing 0 retained)");
    assert.equal(record.name, "JIM", "created record has correct name");
    assert.equal(record.amount, "9876543210", "created record has correct amount");

    record = file.findfirstSync();
    expect(record).to.not.be.null;
    assert.equal(record.key, "a1b2c3d4", "record has been created");
    assert.equal(record.name, "JOHN", "created record has correct name");
    assert.equal(record.amount, "1234", "created record has correct amount");

    record = file.findgeSync("43b2c3d0");
    expect(record).to.not.be.null;
    assert.equal(record.key, "a1b2c3d4", "record has been created");
    assert.equal(record.name, "JOHN", "created record has correct name");
    assert.equal(record.amount, "1234", "created record has correct amount");

    record.key = "F1F2F3F4";
    record.name = "TEST ";
    var count = file.writeSync(record);
    assert.equal(count, 1);

    record = file.findSync("F1F2F3F4");
    expect(record).to.not.be.null;
    assert.equal(record.name, "TEST ", "created record has correct name");
    record.name = "U  ";

    var count = file.updateSync(record);
    assert.equal(count, 1);

    record = file.findSync("F1F2F3F4");
    expect(record).to.not.be.null;
    assert.equal(record.name, "U  ");
    assert.equal(record.amount, "1234");

    keybuf = Buffer.from([0xc1, 0xd2, 0xe3, 0xf4]);
    record = {
      key: keybuf.toString('hex'),
      name: "MARY",
      amount: "999"
    };
    count = file.writeSync(record);
    assert.equal(count, 1);

    record = file.findSync(keybuf, keybuf.length);
    expect(record).to.not.be.null;
    assert.equal(record.key, "c1d2e3f4");
    assert.equal(record.name, "MARY");
    assert.equal(record.amount, "9990");

    record.name = "Mary Smith";
    count = file.updateSync(record);
    assert.equal(count, 1);

    record = file.findSync(keybuf, keybuf.length);
    expect(record).to.not.be.null;
    assert.equal(record.name, "Mary Smith");

    count = file.deleteSync();
    assert.equal(count, 1);

    record = file.findSync("0xF1F2F3F4");
    expect(record).to.not.be.null;
    assert.equal(record.name, "U  ");
    assert.equal(record.amount, "1234");

    count = file.deleteSync();
    assert.equal(count, 1);

    expect(file.close()).to.not.throw;
    done();
  });

  it("write new record after read", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    var record = file.findfirstSync();
    expect(record).to.not.be.null;
    // trailing 00 will be truncated, key up to 'c' matches JIM's, whose key is then followed by 'd'
    record.key = "e5f6789afabc00";
    record.name = "JANE";
    record.amount = "4187832145";

    var count = file.writeSync(record);
    assert.equal(count, 1);

    record = file.findSync("e5f6789afabc");
    expect(record).to.not.be.null;
    assert.equal(record.key, "e5f6789afabc", "record has been created (00 truncated)");
    assert.equal(record.name, "JANE", "created record has correct name");
    assert.equal(record.amount, "4187832145", "created record has correct amount");

    expect(file.close()).to.not.throw;
    done();
  });

  it("write new record with max fields' lengths including 0's", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    var record = file.readSync();
    expect(record).to.not.be.null;
    record.key = "A900B2F4fabc00e9";
    record.name = "M Name TCD";
    record.amount = "A1b2c3D4F6000090";

    var count = file.writeSync(record);
    assert.equal(count, 1);
    record = file.findSync("a900b2F4fabc00E9");
    expect(record).to.not.be.null;
    assert.equal(record.key, "a900b2f4fabc00e9", "record has been created");
    assert.equal(record.name, "M Name TCD", "created record has correct name");
    assert.equal(record.amount, "a1b2c3d4f6000090", "created record has correct amount");

    expect(file.close()).to.not.throw;
    done();
  });

  it("run multiple sync read-only transactions with promise", function() {
    var filerw = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')),
                             "rb+,type=record");
    var filero = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')),
                             "rb,type=record");
    function p1() {
      return new Promise((resolve, reject) => {
        var record = filero.findSync("A900B2F4fabc00e9");
        expect(record).to.not.be.null;
        assert.equal(record.key, "a900b2f4fabc00e9", "6. record has been created");
        assert.equal(record.name, "M Name TCD", "5. created record has correct name");
        assert.equal(record.amount, "a1b2c3d4f6000090", "6. created record has correct amount");

        // write to the same dataset (with filerw) while it's already open in read-mode (with filero);
        // p2() below should also read the updated record
        record.key = "f1234f";
        record.amount = "880022";
        filerw.writeSync(record);
        resolve(0);
      })
    }
    function p2() {
      return new Promise((resolve, reject) => {
        var record = filero.findlastSync();
        expect(record).to.not.be.null;
        assert.equal(record.key, "f1234f");
        assert.equal(record.name, "M Name TCD");
        assert.equal(record.amount, "880022");

        // p3() below should read the following new record after findfirstSync(), using filero
        record.key = "a1b2c3d4e5";
        record.amount = "990033";
        filerw.writeSync(record);
        resolve(0);
      })
    }
    function p3() {
      return new Promise((resolve, reject) => {
        var record = filero.findfirstSync();
        expect(record).to.not.be.null;
        assert.equal(record.key, "a1b2c3d4");
        assert.equal(record.name, "JOHN");
        assert.equal(record.amount, "1234");

        record = filero.readSync();
        assert.equal(record.key, "a1b2c3d4e5");
        assert.equal(record.name, "M Name TCD");
        assert.equal(record.amount, "990033");
        resolve(0);
      })
    }
    function p4() {
      return new Promise((resolve, reject) => {
        var record = filero.findgeSync("43b2c3d0");
        expect(record).to.not.be.null;
        assert.equal(record.key, "a1b2c3d4", "4. record has been created");
        assert.equal(record.name, "JOHN", "created record has correct name");
        assert.equal(record.amount, "1234", "created record has correct amount");
        resolve(0);
      })
    }
    return Promise.all([p1(),p2(),p3(),p4()]).then(res => {
      expect(filero.close()).to.not.throw;
      expect(filerw.close()).to.not.throw;
    }).catch(err => {
      assert.ifError(err);
      expect(filero.close()).to.not.throw;
      expect(filerw.close()).to.not.throw;
    })
  });

  it("delete existing record, then find it and verify error, then delete it and verify error", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
//  const keybuf = Buffer.from([0xa1, 0xb2, 0xc3, 0xd4]);
    const keybuf = Buffer.from([0xA1, 0xB2, 0xc3, 0xD4]);
    var record = file.findSync(keybuf, keybuf.length);
    expect(record).to.not.be.null;
    assert.equal(record.key, "a1b2c3d4", "record has been created");
    assert.equal(record.name, "JOHN", "created record has correct name");
    assert.equal(record.amount, "1234", "created record has correct amount");

    var count = file.deleteSync();
    assert.equal(count, 1);

    expect(() => {
      count = file.findSync("a1b2c3d4");
      assert.equal(count, 0); //no record found
    }).to.not.throw;

    expect(() => {
      count = file.deleteSync("a1b2c3d4");
      assert.equal(count, 0); //no record found
    }).to.not.throw;

    expect(file.close()).to.not.throw;
    done();
  });

  it("reads all records until the end", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    readUntilEnd(file, done);
  });

  it("update existing record and delete it", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    var record = file.findSync("e5f6789afabc");
    expect(record).to.not.be.null;
    record.name = "KEVIN";
    record.amount = "678123";

    var count = file.updateSync(record);
    assert.equal(count, 1);

    record = file.findSync("e5f6789afabc");
    expect(record).to.not.be.null;
    assert.equal(record.name, "KEVIN", "name was not updated");
    assert.equal(record.amount, "678123", "amount was not updated");

    count = file.deleteSync();
    assert.equal(count, 1);

    expect(file.close()).to.not.throw;
    done();
  });

  it("leave a record field undefined for write and update", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    var record = {
      key: "F1F2F3F4",
      name: "TEST"
      // leave amount undefined, should be set to all 0x00
    };
    var count = file.writeSync(record);
    assert.equal(count, 1);

    var record2 = file.findSync(record.key);
    expect(record2).to.not.be.null;
    assert.equal(record2.key, "f1f2f3f4", "correct key in record");
    assert.equal(record2.name, "TEST", "correct name in record");
    assert.equal(record2.amount, "00", "default amount set to 0 as expected");

    var record3 = {
      key: "F1F2F3F4",
      name: "TEST"
      // leave amount undefined, update should fail
    };
    expect(() => {
      count = file.updateSync(record3);
    }).to.throw(/updateSync error: update value for amount has not been set./);

    var record4 = {
      key: "F1F2F3F4",
      amount: "0x1234"
      // leave name undefined, write should fail because minLength=1
    };
    expect(() => {
      count = file.writeSync(record4);
    }).to.throw(/writeSync error: length of 'name' must be 1 or more./);

    expect(file.close()).to.not.throw;
    done();
  });

  it("find and update in one call, also verify no record found case", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    var record = {
      // leave key and name undefined, so only amount is updated in the found record
      amount: "0123456789000198"
    };
    var count = file.updateSync("f1f2f3f4", record);
    assert.equal(count, 1);
    expect(() => {
      count = file.updateSync("f1f2f3f499", record); //no record found
      assert.equal(count, 0);
    }).to.not.throw;

    expect(file.close()).to.not.throw;
    done();
  });

  it("find and update in one call, also verify no record found case", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    // verify record previously updated after file close/open
    var record = file.findSync("f1f2f3f4");
    expect(record).to.not.be.null;
    assert.equal(record.key, "f1f2f3f4", "correct key");
    assert.equal(record.name, "TEST", "correct name");
    assert.equal(record.amount, "0123456789000198", "correct amount");

    // find and update another record with key-buf, key-buflen:
    record2 = {
      name: "diane",
      amount: "12"
    };
    const keybuf = Buffer.from([0xA9, 0, 0xB2, 0xF4, 0xfa, 0xbc, 0, 0xe9]);
    var count = file.updateSync(keybuf, keybuf.length, record2);
    assert.equal(count, 1);

    // independant test (of above) for no record found
    expect(() => {
      count = file.updateSync("f1f2f3f4f5", record2);
      assert.equal(count, 0);
    }).to.not.throw; //no record found with the key for update

    expect(file.close()).to.not.throw;
    done();
  });

  it("find and delete in one call", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    var count = file.deleteSync("e5f6789afabc");
    assert.equal(count, 1);
    expect(file.close()).to.not.throw;
    done();
  });

  it("find and delete in one call, also verify no record found case", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    // verify record previously deleted with find-delete after file close/open
    expect(() => {
      var record = file.findSync("e5f6789afabc");
      assert.equal(record, null);
    }).to.not.throw; //no record found

    const keybuf = Buffer.from([0xe5, 0xf6, 0x78, 0x9a, 0xfa, 0xbc, 0xd0]);
    var count = file.deleteSync(keybuf, keybuf.length);
    assert.equal(count, 0);

    // find and delete another record with key-string:
    count = file.deleteSync("f1f2f3f4");
    assert.equal(count, 1);

    // independant test (of above) for no record found
    expect(() => {
      count = file.deleteSync("f1f2f3f4f5f6");
      assert.equal(count, 0);
    }).to.not.throw; //no record found with the key for delete

    expect(file.close()).to.not.throw;
    done();
  });

  it("reads all records until the end", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    readUntilEnd(file, done);
  });

  it("write new records to test multiple find-update and find-delete", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    var record = { key: "f1f2f3f4f5f6f7f8", name: "name 12341", amount: "f1f2f3f4f5f6f7f8" };
    var count = file.writeSync(record);
    assert.equal(count, 1);

    record = { key: "a1a2a3a4a5a6a7a8", name: "name 12341", amount: "a1f2f3f4f5f6f7f8" };
    count = file.writeSync(record);
    assert.equal(count, 1);

    record = { key: "a1a2a3a4b5b6b7b8", name: "name 2342", amount: "b1b2b3b4f5f6f7f8" };
    count = file.writeSync(record);
    assert.equal(count, 1);

    record = { key: "c1c2c3c4a5a6a7a8", name: "name 32343", amount: "b1b2b3f4c5f6f7f8" };
    count = file.writeSync(record);
    assert.equal(count, 1);

    record = { key: "b1b2b3b4c5b6b7b8", name: "name 42344", amount: "c1c2c3c4f5f6f7f8" };
    count = file.writeSync(record);
    assert.equal(count, 1);

    record = { key: "a1a2a3a4", name: "name 52345", amount: "d1d2d3f4f5f6f7f8" };
    file.writeSync(record);
    assert.equal(count, 1);

    record = { key: "a1a2a3a4b5b6b7", name: "name 62", amount: "e1b2b3b4f5f6f7f8" };
    file.writeSync(record);
    assert.equal(count, 1);

    record = { key: "c1c2c3c4a5a", name: "name 72347", amount: "f1a2a3f4c5" };
    file.writeSync(record);
    assert.equal(count, 1);

    record = { key: "b1b2b3b4", name: "name 82348", amount: "c1c2c3c4f5f6f7f8" };
    file.writeSync(record);
    assert.equal(count, 1);

    expect(file.close()).to.not.throw;
    done();
  });

  it("write more new records to test multiple find-update and find-delete", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    var record = { key: "f1f2f3f4", name: "name 12341", amount: "" };
    var count = file.writeSync(record);
    assert.equal(count, 1);

    record = { key: "f1f2f3f4f5f6f7", name: "NAME 12341", amount: "a1f2f3f400f6f7" };
    count = file.writeSync(record);
    assert.equal(count, 1);

    record = { key: "f1f2f3f4b5b6b7b8", name: "NAME 2342", amount: "00b3b4f5f6f7f8" };
    count = file.writeSync(record);
    assert.equal(count, 1);

    record = { key: "c2c3c4a5a6a7a8", name: "NAME 32343", amount: "00b2b3f4c5f6f7f8" };
    count = file.writeSync(record);
    assert.equal(count, 1);

    record = { key: "b3b4c5b6b7b8", name: "NAME 42344", amount: "c3c4f5f6f7f8" };
    count = file.writeSync(record);
    assert.equal(count, 1);

    record = { key: "a1", name: "NAME 52345", amount: "d1d2d3f4f5f6f7f8" };
    count = file.writeSync(record);
    assert.equal(count, 1);

    record = { key: "a2", name: "NAME 62", amount: "e1b2b3b4f5f6f7f8" };
    count = file.writeSync(record);
    assert.equal(count, 1);

    record = { key: "a3", name: "NAME 72347", amount: "f1a2a3f4c5" };
    count = file.writeSync(record);
    assert.equal(count, 1);

    record = { key: "b1b2", name: "NAME 82348", amount: "c3c4f5f6f7f8" };
    count = file.writeSync(record);
    assert.equal(count, 1);

    expect(file.close()).to.not.throw;
    done();
  });

  it("test updating and deleting multiple records", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    var record = { name: "12341 nam", amount: "00234567890abcde" };
    var count = file.updateSync("f1f2f3f4", record);
    assert.equal(count, 4);

    count = file.updateSync("a100000000000000", record);
    assert.equal(count, 1);

    count = file.deleteSync("a1a2a3a4b5");
    assert.equal(count, 2);

    count = file.updateSync("a1a2a3a4", record);
    assert.equal(count, 2);

    record = file.findSync("a1a2a3a4");
    assert.equal(record.key, "a1a2a3a4");
    assert.equal(record.name, "12341 nam");
    assert.equal(record.amount, "00234567890abcde");

    record = file.readSync();
    assert.equal(record.key, "a1a2a3a4a5a6a7a8");
    assert.equal(record.name, "12341 nam");
    assert.equal(record.amount, "00234567890abcde");

    count = file.deleteSync("b1b2");
    assert.equal(count, 3);

    record = file.findSync("b1b2");
    assert.equal(record, null);

    count = file.deleteSync("b3b4c5b6b7b8");
    assert.equal(count, 1);

    count = file.deleteSync("f1f2f3f4");
    assert.equal(count, 4);

    count = file.deleteSync("a1");
    assert.equal(count, 4);

    count = file.deleteSync("c1");
    assert.equal(count, 2);

    count = file.deleteSync("a2");
    assert.equal(count, 1);
    expect(file.close()).to.not.throw;
    done();
  });

  it("test update error during validation followed by a valid update", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    var record = { amount: "123" };
    expect(() => {
      var count = file.updateSync("f1f2f3f4f5f6f7f8f9", record);
    }).to.throw(/updateSync error: number of hex digits 9 for 'key' exceed schema's length 8, found <f1f2f3f4f5f6f7f8f9>./);

    var count = file.updateSync("a900b2f4fabc00e9", record);
    assert.equal(count, 1);

    expect(file.close()).to.not.throw;
    done();
  });

  it("reads all records until the end", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')),
                             'rb,type=record');
    readUntilEnd(file, done);
  });

  it("run sync find-update transactions with promise", function() {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));

    function p1() {
      return new Promise((resolve, reject) => {
        var record = { name: "UPD c2c3 N", amount: "c5f6f7f800b2b3f4" };
        var count = file.updateSync("c2c3c4a5a6a7a8", record);
        assert.equal(count, 1);
        resolve(0);
      })
    }
    function p2() {
      return new Promise((resolve, reject) => {
        var record = { name: "diana", amount: "c5c4c3c2c1c" };
        var count = file.updateSync("a900", record);
        assert.equal(count, 1);
        resolve(0);
      })
    }
    function p3() {
      return new Promise((resolve, reject) => {
        var count = file.deleteSync("a3");
        assert.equal(count, 1);
        resolve(0);
      })
    }
    return Promise.all([p1(),p2(),p3()]).then(res => {
      expect(file.close()).to.not.throw;
    }).catch(err => {
      assert.ifError(err);
      expect(file.close()).to.not.throw;
    })
  });

  it("write back a records to enable above test run in a loop if needed", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    var record = { key: "a3", name: "NAME 72347", amount: "f1a2a3f4c5" };
    var count = file.writeSync(record);
    assert.equal(count, 1);

    expect(file.close()).to.not.throw;
    done();
  });

  it("reads all records until the end", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')),
                             'rb,type=record');
    readUntilEnd(file, done);
  });

  it("write more new records to test multiple find-delete", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    record = { key: "f0f2f3f4", name: "name 12341", amount: "" };
    count = file.writeSync(record);
    assert.equal(count, 1);

    record = { key: "f0f2f3f4f5f6f7", name: "NAME 12341", amount: "a1f2f3f400f6f7" };
    count = file.writeSync(record);
    assert.equal(count, 1);

    record = { key: "f0f2f3f4b5b6b7b8", name: "NAME 2342", amount: "00b3b4f5f6f7f8" };
    count = file.writeSync(record);
    assert.equal(count, 1);

    record = { key: "c0c3c4a5a6a7a8", name: "NAME 32343", amount: "00b2b3f4c5f6f7f8" };
    count = file.writeSync(record);
    assert.equal(count, 1);

    record = { key: "b0b4c5b6b7b8", name: "NAME 42344", amount: "c3c4f5f6f7f8" };
    count = file.writeSync(record);
    assert.equal(count, 1);

    record = { key: "a1", name: "NAME 52345", amount: "d1d2d3f4f5f6f7f8" };
    count = file.writeSync(record);
    assert.equal(count, 1);

    record = { key: "a2222222", name: "NAME 62", amount: "e1b2b3b4f5f6f7f8" };
    count = file.writeSync(record);
    assert.equal(count, 1);

    record = { key: "a333333333333333", name: "NAME 72347", amount: "f1a2a3f4c5" };
    count = file.writeSync(record);
    assert.equal(count, 1);

    record = { key: "b1b2999999999998", name: "NAME 82348", amount: "c3c4f5f6f7f8" };
    count = file.writeSync(record);
    assert.equal(count, 1);

    expect(file.close()).to.not.throw;
    done();
  });

  it("run sync find-delete transactions with promise", function() {
    function p1() {
      return new Promise((resolve, reject) => {
        var file = vsam.openSync(testSet,
                                  JSON.parse(fs.readFileSync('test/test3.json')));
        var record = file.findSync("c0c3c4a5a6a7a8");
        expect(record).to.not.be.null;
        var count = file.deleteSync("c0c3c4a5a6a7a8");
        assert.equal(count, 1);
        expect(file.close()).to.not.throw;
        resolve(0);
      })
    }
    function p2() {
      return new Promise((resolve, reject) => {
        var file = vsam.openSync(testSet,
                                 JSON.parse(fs.readFileSync('test/test3.json')));
        var count = file.deleteSync("f0f2f3f4b5b6b7b8");
        expect(record).to.not.be.null;
        assert.equal(count, 1);
        expect(file.close()).to.not.throw;
        resolve(0);
      })
    }
    function p3() {
      return new Promise((resolve, reject) => {
        var file = vsam.openSync(testSet,
                                 JSON.parse(fs.readFileSync('test/test3.json')));
        var count = file.deleteSync("a1");
        assert.equal(count, 1);
        expect(file.close()).to.not.throw;
        resolve(0);
      })
    }
    function p4() {
      return new Promise((resolve, reject) => {
        var file = vsam.openSync(testSet,
                                 JSON.parse(fs.readFileSync('test/test3.json')));
        var count = file.deleteSync("b0b4c5b6b7b8");
        assert.equal(count, 1);
        expect(file.close()).to.not.throw;
        resolve(0);
      })
    }
    function p5() {
      return new Promise((resolve, reject) => {
        var file = vsam.openSync(testSet,
                                 JSON.parse(fs.readFileSync('test/test3.json')));
        var count = file.deleteSync("a333333333333333");
        assert.equal(count, 1);
        expect(file.close()).to.not.throw;
        resolve(0);
      })
    }
    function p6() {
      return new Promise((resolve, reject) => {
        var file = vsam.openSync(testSet,
                                 JSON.parse(fs.readFileSync('test/test3.json')));
        var count = file.deleteSync("a2222222");
        assert.equal(count, 1);
        expect(file.close()).to.not.throw;
        resolve(0);
      })
    }
    function p7() {
      return new Promise((resolve, reject) => {
        var file = vsam.openSync(testSet,
                                 JSON.parse(fs.readFileSync('test/test3.json')));
        var count = file.deleteSync("f0f2f3f4f5f6f7");
        assert.equal(count, 1);
        expect(file.close()).to.not.throw;
        resolve(0);
      })
    }
    function p8() {
      return new Promise((resolve, reject) => {
        var file = vsam.openSync(testSet,
                                 JSON.parse(fs.readFileSync('test/test3.json')));
        var count = file.deleteSync("f0f2f3f4");
        assert.equal(count, 1);
        expect(file.close()).to.not.throw;
        resolve(0);
      })
    }
    function p9() {
      return new Promise((resolve, reject) => {
        var file = vsam.openSync(testSet,
                                 JSON.parse(fs.readFileSync('test/test3.json')));
        var count = file.deleteSync("b1b2999999999998");
        assert.equal(count, 1);
        expect(file.close()).to.not.throw;
        resolve(0);
      })
    }

    return Promise.all([p1(),p2(),p3(),p4(),p5(),p6(),p7(),p8(),p9()]).then(res => {
    }).catch(err => {
      assert.ifError(err);
    })
  });

  it("reads all records until the end", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')),
                             'rb,type=record');
    readUntilEnd(file, done);
  });

  it("deallocate a dataset", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/test3.json')));
    expect(file.close()).to.not.throw;
    file.dealloc((err) => {
      assert.ifError(err);
      done();
    });
  });
});

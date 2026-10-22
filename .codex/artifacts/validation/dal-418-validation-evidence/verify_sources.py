import hashlib,json,pathlib
root=pathlib.Path(__file__).resolve().parent
source=root/"source"
expected=json.loads((root/"expected-source-blobs.json").read_text())
different=[]
eol=[]
for name,digest in expected.items():
    data=(source/name).read_bytes()
    actual=hashlib.sha1(b"blob "+str(len(data)).encode()+b"\0"+data).hexdigest()
    if actual != digest:
        normalized=data.replace(b"\r\n",b"\n")
        actual=hashlib.sha1(b"blob "+str(len(normalized)).encode()+b"\0"+normalized).hexdigest()
        if actual==digest:
            eol.append(name)
        else:
            different.append(name)
result={"tested_head":"6372ede9452b2ec912234a373666dda9b239a1bc","verified_blobs":len(expected),"eol_exports":eol,"content_mismatches":different}
(root/"windows-source-audit.json").write_text(json.dumps(result,indent=2)+"\n")
print(json.dumps(result,indent=2))
assert not different,different

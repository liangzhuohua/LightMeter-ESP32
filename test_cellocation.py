import urllib.request
import json
import socket

WL = "a4:39:b3:c8:8c:a0,-25;d8:76:ae:6b:f7:90,-35;02:5c:c2:18:ee:84,-37;00:5c:c2:08:ee:84,-38;04:67:61:73:f7:6a,-46;6c:b1:58:d8:88:dd,-54;6e:b1:58:d8:88:dd,-54;d8:76:ae:6b:e4:f8,-56;f4:84:8d:02:81:8e,-57;d8:76:ae:6b:c6:6c,-60;d8:76:ae:6b:d3:a4,-60;44:f7:70:f8:c8:ba,-60;d8:76:ae:6b:b5:5c,-62;d8:76:ae:6b:ca:8c,-62;d8:76:ae:6b:d5:60,-64"

endpoints = [
    ("混合定位 (port 84, 文档原始端点)", f"http://api.cellocation.com:84/loc/?wl={WL}&coord=wgs84&output=json"),
    ("单WIFI查询 (port 84)", f"http://api.cellocation.com:84/wifi/?mac=a4:39:b3:c8:8c:a0&coord=wgs84&output=json"),
    ("混合定位 (port 80)", f"http://api.cellocation.com/loc/?wl={WL}&coord=wgs84&output=json"),
    ("混合定位 (HTTPS 443)", f"https://api.cellocation.com/loc/?wl={WL}&coord=wgs84&output=json"),
]

# 先检查 DNS 和端口
ip = socket.getaddrinfo("api.cellocation.com", 84)[0][4][0]
print(f"DNS: api.cellocation.com -> {ip}")
for port in [80, 84, 443]:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(3)
    try:
        s.connect((ip, port))
        print(f"  端口 {port}: 开放")
        s.close()
    except Exception as e:
        print(f"  端口 {port}: {e}")
print()

for name, url in endpoints:
    print(f"[{name}]")
    print(f"  URL: {url[:100]}...")
    try:
        req = urllib.request.Request(url)
        with urllib.request.urlopen(req, timeout=10) as resp:
            data = resp.read().decode("utf-8")
            print(f"  状态码: {resp.status}")
            try:
                print(f"  响应: {json.dumps(json.loads(data), indent=2, ensure_ascii=False)}")
            except:
                print(f"  响应: {data[:300]}")
    except Exception as e:
        print(f"  失败: {type(e).__name__}: {e}")
    print()

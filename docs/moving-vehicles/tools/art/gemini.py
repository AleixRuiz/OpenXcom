"""Talks to Google's image models with the key in ~/.gemini-api-key. The key is only ever sent in a
request header, never in a URL, and is scrubbed from anything printed."""
import base64
import json
import os
import urllib.error
import urllib.request

KEY = open(os.path.join(os.path.expanduser('~'), '.gemini-api-key'), encoding='utf-8').read().strip()

ENDPOINTS = {
    'gemini': 'https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent',
    'vertex': 'https://aiplatform.googleapis.com/v1/publishers/google/models/{model}:generateContent',
}


def safe(text):
    return str(text).replace(KEY, '***')


def call(url, body=None, timeout=180):
    """Returns (status, parsed JSON or error text)."""
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(url, data=data, method='POST' if data else 'GET',
                                 headers={'x-goog-api-key': KEY, 'Content-Type': 'application/json'})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.status, json.loads(r.read().decode())
    except urllib.error.HTTPError as e:
        return e.code, safe(e.read().decode(errors='replace'))
    except Exception as e:  # network trouble
        return -1, safe(e)


def png_part(path):
    return {'inlineData': {'mimeType': 'image/png', 'data': base64.b64encode(open(path, 'rb').read()).decode()}}


def generate(endpoint, model, parts, out_prefix):
    """Asks for an image; saves every image returned as out_prefix_N.png. Returns (status, text, files)."""
    body = {'contents': [{'role': 'user', 'parts': parts}],
            'generationConfig': {'responseModalities': ['TEXT', 'IMAGE']}}
    status, res = call(ENDPOINTS[endpoint].format(model=model), body)
    files, text = [], ''
    if status == 200:
        for cand in res.get('candidates', []):
            for part in cand.get('content', {}).get('parts', []):
                if 'inlineData' in part:
                    path = '%s_%d.png' % (out_prefix, len(files))
                    open(path, 'wb').write(base64.b64decode(part['inlineData']['data']))
                    files.append(path)
                elif 'text' in part:
                    text += part['text']
        return status, safe(text), files
    return status, safe(res)[:600], files

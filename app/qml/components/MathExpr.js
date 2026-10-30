.pragma library

// Expresii aritmetice în câmpurile numerice: „1000 - 30”, „250*4”, „(1200+300)/2”, „-2,5”. Punct sau virgulă ca
// separator zecimal; operatorii + − × / și paranteze, cu prioritatea obișnuită. Fără eval: un text care nu e o
// expresie întreagă și corectă (sau împarte la zero) dă NaN.

function evaluate(text) {
    const tokens = tokenize(String(text))
    if (tokens === null || tokens.length === 0)
        return NaN
    let i = 0

    const peek = () => tokens[i]
    const next = () => tokens[i++]

    // expresie := termen (('+' | '-') termen)*
    function expression() {
        let v = term()
        while (peek() === "+" || peek() === "-")
            v = next() === "+" ? v + term() : v - term()
        return v
    }
    // termen := factor (('*' | '/') factor)*
    function term() {
        let v = factor()
        while (peek() === "*" || peek() === "/") {
            if (next() === "*") {
                v *= factor()
            } else {
                const d = factor()
                v = d === 0 ? NaN : v / d
            }
        }
        return v
    }
    // factor := ('+' | '-') factor | număr | '(' expresie ')'
    function factor() {
        const t = next()
        if (t === "+")
            return factor()
        if (t === "-")
            return -factor()
        if (t === "(") {
            const v = expression()
            return next() === ")" ? v : NaN
        }
        return typeof t === "number" ? t : NaN
    }

    const value = expression()
    return i === tokens.length && isFinite(value) ? value : NaN
}

function tokenize(text) {
    const tokens = []
    const re = /\s*(?:(\d+(?:[.,]\d*)?|[.,]\d+)|([-+*/()]))/y
    let pos = 0
    while (pos < text.length) {
        if (/^\s+$/.test(text.slice(pos)))
            break
        re.lastIndex = pos
        const m = re.exec(text)
        if (m === null)
            return null
        tokens.push(m[1] !== undefined ? Number(m[1].replace(",", ".")) : m[2])
        pos = re.lastIndex
    }
    return tokens
}

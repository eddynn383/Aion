.pragma library

// Ghidurile uneltelor: ce face unealta și cum se folosește, afișate la hover lung pe butonul ei
// (ca „rich tooltips” din Photoshop) și în turul aplicației. Animația corespunzătoare e în ToolDemo.qml.
const guides = {
    select: {
        title: "Select",
        summary: "Alege muchii, fețe, grupuri și obiecte, ca să lucrezi cu ele.",
        steps: ["Click pe o față sau o muchie o selectează", "Dublu-click: fața cu muchiile ei; triplu-click: tot ce e conectat",
                "Trage spre dreapta: doar ce e complet în dreptunghi; spre stânga: tot ce atinge"],
        tip: "Shift adaugă sau scoate din selecție"
    },
    line: {
        title: "Line",
        summary: "Desenează muchii. Când liniile închid un contur plan, apare automat o față.",
        steps: ["Click pentru primul punct, apoi click pentru următorul", "Urmează inferențele: capăt, mijloc, axele colorate",
                "Tastează lungimea (de exemplu 3m) și apasă Enter"],
        tip: "Săgețile blochează direcția pe o axă; Esc oprește linia"
    },
    arc: {
        title: "Arc",
        summary: "Desenează un arc din două capete și o săgeată (cât de bombat e).",
        steps: ["Click pe primul capăt, apoi pe al doilea", "Mișcă mouse-ul ca să alegi bombarea, apoi click",
                "Tastează săgeata sau numărul de segmente (de exemplu 12s)"],
        tip: "Arcul e o singură curbă: Eraser îl șterge întreg"
    },
    rectangle: {
        title: "Rectangle",
        summary: "Desenează un dreptunghi din două colțuri opuse; fața apare imediat.",
        steps: ["Click pentru primul colț", "Mișcă mouse-ul și dă click pentru colțul opus",
                "Sau tastează lățimea și lungimea: 3000,2000"],
        tip: "Pe o față existentă, dreptunghiul o împarte"
    },
    circle: {
        title: "Circle",
        summary: "Desenează un cerc (ca poligon cu multe laturi) cu o față în interior.",
        steps: ["Click pentru centru", "Mișcă mouse-ul pentru rază și dă click", "Tastează raza sau numărul de laturi (de exemplu 48s)"],
        tip: "Push/Pull pe un cerc face un cilindru neted"
    },
    polygon: {
        title: "Polygon",
        summary: "Desenează un poligon regulat (implicit hexagon).",
        steps: ["Click pentru centru", "Mișcă mouse-ul pentru rază și dă click", "Tastează numărul de laturi urmat de s (8s)"],
        tip: "Muchiile poligonului rămân separate, spre deosebire de cerc"
    },
    cube: {
        title: "Cub",
        summary: "Creează direct un obiect cub (cu fețele lui), gata de mutat, rotit sau redimensionat.",
        steps: ["Click pentru centrul bazei (pe o față: în planul ei)", "Mișcă mouse-ul pentru lățime și dă click",
                "Mișcă mouse-ul pentru înălțime (se prinde în dreptul punctelor din jur) și dă click",
                "Sau tastează lățimea (1000) ori lățimea și înălțimea (1000,500)"],
        tip: "Dimensiunile se pot schimba apoi din Inspector"
    },
    sphere: {
        title: "Sferă",
        summary: "Creează un obiect sferă, cu suprafața netedă, așezat pe planul ales.",
        steps: ["Click pentru centrul bazei", "Mișcă mouse-ul pentru diametru și dă click", "Sau tastează diametrul"],
        tip: ""
    },
    cone: {
        title: "Con",
        summary: "Creează un obiect con, cu baza pe planul ales.",
        steps: ["Click pentru centrul bazei", "Mișcă mouse-ul pentru lățime (diametru) și dă click",
                "Mișcă mouse-ul pentru înălțime și dă click", "Sau tastează diametrul și înălțimea (600,1200)"],
        tip: ""
    },
    cylinder: {
        title: "Cilindru",
        summary: "Creează un obiect cilindru, cu baza pe planul ales.",
        steps: ["Click pentru centrul bazei", "Mișcă mouse-ul pentru lățime (diametru) și dă click",
                "Mișcă mouse-ul pentru înălțime și dă click", "Sau tastează diametrul și înălțimea (600,1500)"],
        tip: ""
    },
    pushpull: {
        title: "Push/Pull",
        summary: "Trage o față în sus sau în jos și transformă formele plane în volume.",
        steps: ["Click pe o față", "Mișcă mouse-ul pentru înălțime și dă click", "Sau tastează distanța (1.5m) și apasă Enter"],
        tip: "Dublu-click repetă ultima distanță; Option (⌥) păstrează fața de pornire"
    },
    offset: {
        title: "Offset",
        summary: "Copiază conturul unei fețe la o distanță constantă, spre interior sau spre exterior.",
        steps: ["Click pe o față", "Mișcă mouse-ul spre interior sau spre exterior, apoi click", "Sau tastează distanța"],
        tip: "Util pentru rame, borduri și pereți cu grosime"
    },
    followme: {
        title: "Follow Me",
        summary: "Va extruda un profil de-a lungul unui drum (muluri, țevi, borduri).",
        steps: ["În curând"],
        tip: ""
    },
    move: {
        title: "Move",
        summary: "Mută selecția sau, cu Option (⌥), o copiază.",
        steps: ["Click pe un punct de pornire", "Mișcă de-a lungul unei axe și dă click", "Tastează distanța; după copiere, x3 face trei copii"],
        tip: "Mutând o muchie sau un vârf, formele se deformează (acoperișuri, pante)"
    },
    rotate: {
        title: "Rotate",
        summary: "Rotește selecția în jurul unui punct, cu raportorul.",
        steps: ["Click pe centrul rotației", "Click pe direcția de referință", "Rotește și dă click, sau tastează unghiul (30)"],
        tip: "Culoarea raportorului arată planul: albastru = orizontal"
    },
    scale: {
        title: "Scale",
        summary: "Mărește sau micșorează selecția: uniform, pe două axe sau pe una.",
        steps: ["Click pe un mâner: colț = uniform, mijlocul unei muchii = două axe, centrul unei fețe = o axă",
                "Mișcă mouse-ul și dă click", "Sau tastează factorul: 2, 0.5 ori 2,1,1.5 (X, Y, Z)"],
        tip: "Punctul opus mânerului rămâne pe loc; cu Option scalezi din centru, iar un factor negativ oglindește"
    },
    tape: {
        title: "Tape Measure",
        summary: "Măsoară distanțe și creează ghidaje pentru desen precis.",
        steps: ["Click pe o muchie și trage: apare un ghidaj paralel", "Click pe un punct: ghidaj punct",
                "Tastează distanța ghidajului"],
        tip: "Ghidajele participă la inferență; le ștergi din Editare → Șterge ghidajele"
    },
    protractor: {
        title: "Protractor",
        summary: "Creează ghidaje la un unghi dat.",
        steps: ["Click pe vârful unghiului", "Click pe linia de referință", "Rotește și dă click, sau tastează unghiul"],
        tip: ""
    },
    dimension: {
        title: "Dimension",
        summary: "Adaugă cote care rămân în model și se actualizează singure.",
        steps: ["Click pe primul punct", "Click pe al doilea punct", "Trage cota în afară și dă click"],
        tip: "Unitatea cotelor se schimbă din Preferințe (⌘,)"
    },
    text: {
        title: "Text",
        summary: "Pune o notă cu săgeată pe model.",
        steps: ["Click pe punctul de notat", "Click unde vrei eticheta", "Scrie textul și apasă Enter"],
        tip: "Dublu-click pe o notă o editează"
    },
    section: {
        title: "Section Plane",
        summary: "Taie modelul cu un plan, ca să vezi interiorul (planuri, secțiuni).",
        steps: ["Click pe o față: planul se așază paralel cu ea", "Mută planul din bara viewport-ului (iconița Secțiuni)",
                "Activează sau dezactivează tăietura oricând"],
        tip: ""
    },
    eraser: {
        title: "Eraser",
        summary: "Șterge muchii (și fețele care depind de ele).",
        steps: ["Click pe o muchie", "Sau ține apăsat și trece peste mai multe muchii"],
        tip: "Ștergerea unei muchii dintre două fețe coplanare le unește"
    },
    paint: {
        title: "Paint Bucket",
        summary: "Aplică materiale pe fețe, grupuri și obiecte.",
        steps: ["Alege un material din panoul Materiale", "Click pe o față"],
        tip: "⌥ ia materialul de pe o față; ⇧ înlocuiește peste tot același material"
    },
    interact: {
        title: "Interact",
        summary: "Deschide și închide ușile, sertarele și clapetele cărora le-ai dat o interacțiune.",
        steps: ["Selectează un grup și alege în Entity Info → Interacțiune: Balama sau Glisare",
                "Alege latura balamalei (sau direcția) și unghiul (sau distanța)",
                "Cu Interact, click pe obiect: se deschide; încă un click: se închide"],
        tip: "Starea deschis / închis se salvează în fișier și are undo"
    },
    orbit: {
        title: "Orbit",
        summary: "Rotește camera în jurul modelului.",
        steps: ["Trage cu mouse-ul", "Sau oricând: butonul din mijloc / două degete pe trackpad"],
        tip: "Gizmo-ul din colț duce la vederile standard"
    },
    pan: {
        title: "Pan",
        summary: "Deplasează vederea fără să o rotești.",
        steps: ["Trage cu mouse-ul", "Sau oricând: Shift + butonul din mijloc"],
        tip: ""
    },
    zoom: {
        title: "Zoom",
        summary: "Apropie sau depărtează camera.",
        steps: ["Trage în sus sau în jos", "Sau rotița mouse-ului / pinch pe trackpad"],
        tip: "Shift+Z încadrează tot modelul"
    }
}

function guide(id) {
    return guides[id] || null
}

// Ordinea din turul aplicației.
const tourOrder = ["select", "line", "rectangle", "cube", "pushpull", "move", "rotate", "scale", "offset", "arc", "circle",
                   "polygon", "eraser", "paint", "tape", "protractor", "dimension", "text", "section", "interact", "orbit", "pan", "zoom"]

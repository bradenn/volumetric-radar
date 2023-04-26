import {v4 as uuidv4} from "uuid";

export class Canvas {

    prefix: string
    uuid: string
    canvas: HTMLCanvasElement
    ctx: CanvasRenderingContext2D
    animationFrame: number
    draw: (ctx: CanvasRenderingContext2D) => void

    constructor(identifier: string, render: (ctx: CanvasRenderingContext2D) => void) {
        this.prefix = identifier
        this.uuid = uuidv4()
        this.canvas = {} as HTMLCanvasElement
        this.ctx = {} as CanvasRenderingContext2D
        this.animationFrame = 0
        this.draw = render
    }

    destroy() {
        cancelAnimationFrame(this.animationFrame)

    }

    init() {
        let canvas = document.getElementById(this.Id) as HTMLCanvasElement;
        if (canvas == null) {
            console.log("FAILED", this.Id)
            return
        }
        let ctx = canvas.getContext("2d") as CanvasRenderingContext2D
        let scale = 2
        ctx.scale(scale * 2, scale * 2)

        canvas.width = canvas.clientWidth * scale
        canvas.height = canvas.clientHeight * scale

        this.canvas = canvas
        this.ctx = ctx
        this.render()
    }

    get Id(): string {
        return `${this.prefix}-${this.uuid}`
    }

    paint() {

    }

    render() {
        let ctx = this.ctx
        if (!ctx.canvas) return
        // ctx.lineWidth = 2
        ctx.clearRect(0, 0, ctx.canvas.width, ctx.canvas.height);
        // drawLegend()
        let w = ctx.canvas.width;
        let h = ctx.canvas.height;
        ctx.fillStyle = "rgba(255,255,255,1)";
        if (this.draw) {
            this.draw(ctx)
        }

    }

    origin(ctx: CanvasRenderingContext2D) {


    }

    compass(ctx: CanvasRenderingContext2D, size: number) {
        const w = size;
        const innerRing = w * 0.75;
        ctx.lineWidth = 5
        ctx.lineCap = "round"
        ctx.font = "500 30px JetBrains Mono"
        let metric = ctx.measureText("Y")
        ctx.beginPath()
        ctx.strokeStyle = "rgba(48,209,88,1)"
        ctx.fillStyle = "rgba(48,209,88,1)"
        ctx.moveTo(0, 0)
        ctx.lineTo(0, -w + metric.fontBoundingBoxAscent + 5)
        ctx.stroke()
        ctx.closePath()


        ctx.fillText("Y", -metric.width / 2, -w + metric.actualBoundingBoxAscent)


        metric = ctx.measureText("X")
        ctx.beginPath()
        ctx.strokeStyle = "rgba(255,55,95,1)"
        ctx.fillStyle = "rgba(255,55,95,1)"
        ctx.moveTo(0, 0)
        ctx.lineTo(w - metric.width - 8, 0)
        ctx.stroke()
        ctx.closePath()

        ctx.fillText("X", w - metric.width, +metric.actualBoundingBoxAscent / 2)


        ctx.beginPath()
        ctx.strokeStyle = "rgba(10,132,255,1)"
        ctx.moveTo(0, 0)
        ctx.lineTo(0, 0)
        ctx.stroke()
        ctx.closePath()

        // ctx.font = "bold 30px Jetbrains Mono"
        // let metric = ctx.measureText("N")
        // ctx.fillText("N", -metric.width / 2, -w + metric.fontBoundingBoxDescent / 3)
        // metric = ctx.measureText("S")
        // ctx.fillText("S", -metric.width / 2, w + metric.fontBoundingBoxDescent)
        //
        // metric = ctx.measureText("W")
        // ctx.fillText("W", -metric.width / 2 - w, metric.fontBoundingBoxAscent / 3)
        //
        // metric = ctx.measureText("E")
        // ctx.fillText("E", -metric.width / 2 + w, metric.fontBoundingBoxAscent / 3)

        // Cross
        // for (let i = 0; i < 1; i++) {
        //     ctx.moveTo(0, 0)
        //     ctx.lineTo(Math.cos((Math.PI * 2) * i / 8) * innerRing, Math.sin((Math.PI * 2) * i / 8) * innerRing)
        //     // ctx.stroke()
        // }

        // ctx.stroke()
        // ctx.closePath()
    }

}

export interface Point {
    x: number
    y: number
}

const color = '255,128,10'

const colorAccent0 = `rgba(${color}, 0.2)`
const colorAccent1 = `rgba(${color}, 0.4)`
const colorAccent2 = `rgba(${color}, 0.8)`
const colorAccent3 = `rgba(${color}, 0.8)`
const colorPrimary = `rgba(255, 255, 255, 0.8)`

export class Space {

    width: number
    height: number
    size: number
    verticies: Point[]

    center: Point

    editMode: boolean

    cursor: {
        x: number
        y: number
    }

    constructor(width: number, height: number, size: number) {
        this.width = width
        this.height = height
        this.size = size
        this.cursor = {
            x: 0,
            y: 0,
        }
        this.center = {} as Point
        this.editMode = true
        this.verticies = []
    }

    getPoints(): Point[] {
        return this.verticies
    }

    computeCenter(): void {
        let vcs = this.verticies
        let maxX = Math.max(...vcs.map(v => v.x))
        let maxY = Math.max(...vcs.map(v => v.y))
        let minX = Math.min(...vcs.map(v => v.x))
        let minY = Math.min(...vcs.map(v => v.y))
        this.center = {
            x: (maxX + minX) / 2,
            y: (maxY + minY) / 2,
        } as Point
    }

    clear() {
        this.verticies = []
        this.editMode = true

    }

    setEditMode(edit: boolean) {
        this.editMode = edit
    }

    toggleVertex(x: number, y: number): void {
        if (!this.editMode) return
        let point = this.mapToGrid(x, y)
        let find = this.verticies.find(v => v.x === point.x && v.y === point.y)
        if (find) {
            if (this.verticies.indexOf(find) == 0) {
                this.verticies.push(point)
                this.editMode = false
            } else {
                this.verticies = this.verticies.filter(v => !(v.x === point.x && v.y === point.y))
            }
        } else {
            this.verticies.push(point)
        }
        this.computeCenter()
    }

    toggleVertexOff(x: number, y: number): void {
        let point = this.mapToGrid(x, y)
        if (this.verticies.find(v => v.x == point.x && v.y == point.y)) {
            this.verticies = this.verticies.filter(v => v.x !== point.x && v.y !== point.y)
        }
    }

    cursorVertexExists(): number {
        if (!this.cursor) return 0
        let vertex = this.mapToGrid(this.cursor.x, this.cursor.y)
        let search = this.verticies.find(v => vertex.x == v.x && vertex.y == v.y)
        if (!search) return -1
        return this.verticies.indexOf(search)
    }

    moveCursor(x: number, y: number): void {
        this.cursor.x = x;
        this.cursor.y = y;
    }

    mapToGrid(x: number, y: number): Point {
        const xs = this.width

        const ys = this.height


        const count = this.size;
        const points = [1, -0.5, 0, -1.5]
        const w = Math.ceil((xs / ys) * count)
        const h = count
        const mx = x
        const my = y
        return {
            x: Math.round(mx / (xs / w)),
            y: Math.round(my / (ys / h))
        } as Point
    }

    bezierLerp(t: number, p0: number, p1: number, p2: number, p3: number): number {
        const u = 1 - t;
        const uu = u * u;
        const uuu = uu * u;
        const tt = t * t;
        const ttt = tt * t;
        const p = uuu * p0 + 3 * uu * t * p1 + 3 * u * tt * p2 + ttt * p3;
        return p;
    }

    draw(ctx: CanvasRenderingContext2D) {
        // Define canvas dimensions
        const xs = this.width
        const ys = this.height
        // Define number of cells
        const count = this.size;
        // Define number of x and y cells
        const w = Math.ceil((xs / ys) * count)
        const h = count
        // Define the current cell occupied by the cursor
        const mx = this.cursor.x - (xs / w) / 2
        const my = this.cursor.y - (xs / w) / 2

        const px = Math.round(mx / (xs / w)) * (xs / w);
        const py = Math.round(my / (ys / h)) * (ys / h);

        ctx.lineWidth = 2
        ctx.strokeStyle = `rgba(102, 99, 97, 0.2)`;
        ctx.fillStyle = `rgba(102, 99, 97, 0.8)`;
        ctx.font = '400 12px JetBrains Mono'

        function drawGrid() {
            for (let x = 0; x <= w; x++) {
                ctx.beginPath()
                ctx.moveTo((xs / w) * x, 0);
                ctx.lineTo((xs / w) * x, ys);
                ctx.fillText(`${x}`, (xs / w) * x + 6, 16)
                ctx.stroke()
                ctx.closePath()
            }
            for (let y = 0; y <= h; y++) {
                ctx.beginPath()
                ctx.moveTo(0, (ys / h) * y);
                ctx.lineTo(xs, (ys / h) * y);
                ctx.fillText(`${y}`, 4, (ys / h) * y - 8)
                ctx.stroke()
                ctx.closePath()
            }

        }

        function drawVertices(space: Space) {

            ctx.fillStyle = colorAccent2;
            ctx.font = "500 32px JetBrains Mono"
            ctx.fillText(`${space.editMode ? "Edit" : "View"}`, 20, space.height - 20)

            ctx.lineWidth = 5
            ctx.lineCap = "round"

            ctx.fillStyle = colorAccent0;
            ctx.strokeStyle = colorAccent1

            ctx.beginPath()
            for (let x = 0; x < space.verticies.length; x++) {
                let gx = space.verticies[x].x * (xs / w)
                let gy = space.verticies[x].y * (ys / h)
                ctx.lineTo(gx, gy);
                ctx.fillRect(gx - (xs / w) / 4 / 2, gy - (ys / h) / 4 / 2, (xs / w) / 4, (ys / h) / 4)
                ctx.font = "500 24px JetBrains Mono"
            }
            ctx.stroke()
            ctx.fill()
            ctx.closePath()
        }


        drawGrid()
        drawVertices(this)
        this.drawCursor(ctx)

        // if(this.editMode){
        //     ctx.beginPath()
        //     let last = this.verticies[this.verticies.length-1]
        //     if(!last) return
        //     ctx.moveTo(last.x, last.y)
        //     ctx.lineTo(this.cursor.x, this.cursor.y)
        //     ctx.closePath()
        //     ctx.stroke()
        // }

    }

    private drawCursor(ctx: CanvasRenderingContext2D) {
        if (!this.editMode) return
        let cursorPos = this.mapToGrid(this.cursor.x, this.cursor.y)

        const xs = this.width
        const ys = this.height
        const count = this.size;

        const w = Math.ceil((xs / ys) * this.size)
        const h = count

        const px = cursorPos.x * (xs / w);
        const py = cursorPos.y * (ys / h);

        ctx.lineWidth = 4
        ctx.strokeStyle = colorAccent2;

        // let gx = -this.bezierLerp((this.cursor.x-px)/(xs / w), 1, 0, 1, 0)*(xs / w) / 2
        // let gy = -this.bezierLerp((this.cursor.x-px)/(xs / w), 1, 0, 1, 0)*(xs / w) / 2

        ctx.strokeRect(px - (xs / w) / 4, py - (ys / h) / 4, (xs / w) / 2, (ys / h) / 2)


        ctx.fillStyle = colorAccent2;
        ctx.font = "500 26px JetBrains Mono"

        let char = '';
        let cursor = this.cursorVertexExists()

        if (cursor < 0) {
            char = '+'
            ctx.fillStyle = colorAccent2;
        } else if (cursor === 0) {
            if (this.verticies.length > 1) char = '✓'
            ctx.fillStyle = 'rgba(0,0,0,0.8)';
        } else {
            ctx.fillStyle = 'rgba(0,0,0,0.8)';
            char = '✕'
        }

        let m = ctx.measureText(char)

        // ctx.fillRect(px - (xs / w) / 4, py - (ys / h) / 4, (xs / w) / 2, (ys / h) / 2)

        ctx.save()
        ctx.fillText(char, px - m.width / 2, py + 26 / 3)
        ctx.restore()
    }

}

export function drawCompass() {

}
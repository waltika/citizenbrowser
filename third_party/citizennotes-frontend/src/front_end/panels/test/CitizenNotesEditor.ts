export class CitizenNotesEditor {
  private readonly element: HTMLElement;
  private readonly script: HTMLElement;
  constructor(element: HTMLElement){
    this.element = element;
    let quillElement = this.element.createChild('div');
    quillElement.textContent = 'Tell us what you think...';
    quillElement.id = 'quillEditor';
    quillElement.style.height = '150px';

    this.script = document.createElement("script");
    const inlineScript = document.createTextNode('const quill = new Quill(\'#quillEditor\', {theme: \'snow\'});');
    this.script.appendChild(inlineScript);
  }
  display(){
    this.element.appendChild(this.script);
  }
}

(function() {

// This function stabilize the line number in console messages from this script.
function log(str) {
  console.log(str);
}

class MockFileChooserFactory extends EventTarget {
  constructor() {
    super();
    this.paths_ = [];
    this.bindingSet_ = new mojo.BindingSet(blink.mojom.FileChooser);
    this.interceptor_ = new MojoInterfaceInterceptor(
        blink.mojom.FileChooser.name);
    this.interceptor_.oninterfacerequest = e => {
      this.bindingSet_.addBinding(
          new MockFileChooser(this, this.paths_), e.handle);
      this.paths_ = [];
    };
    this.interceptor_.start();
  }

  setPathsToBeChosen(paths) {
    // TODO(tkent): Need to ensure each of paths is an absolute path.
    this.paths_ = paths;
  }
}

function modeToString(mode) {
  let Mode = blink.mojom.FileChooserParams.Mode;
  switch (mode) {
  case Mode.kOpen:
    return 'kOpen';
  case Mode.kOpenMultiple:
    return 'kOpenMultiple';
  case Mode.kUploadFolder:
    return 'kUploadFolder';
  case Mode.kSave:
    return 'kSave';
  default:
    return `Unknown ${mode}`;
  }
}

class MockFileChooser {
  constructor(factory, paths) {
    this.factory_ = factory;
    this.paths_ = paths;
  }

  openFileChooser(params) {
    this.params_ = params;
    log(`FileChooser: opened; mode=${modeToString(params.mode)}`);
    if (params.mode == blink.mojom.FileChooserParams.Mode.kUploadFolder)
      log('FileChooser: mock-file-chooser.js doesn\'t support directory selection yet.');

    this.factory_.dispatchEvent(new CustomEvent('open'));
    return new Promise((resolve, reject) => {
      setTimeout(() => this.chooseFiles_(resolve), 1);
    });
  }

  enumerateChosenDirectory(directoryPath) {
    console.log('MockFileChooserFactory::EnumerateChosenDirectory() is not implemented.');
  }

  chooseFiles_(resolve) {
    if (this.paths_.length > 0) {
      log('FileChooser: selected: ' + this.paths_);
    } else {
      log('FileChooser: canceled');
    }
    const file_info_list = [];
    for (const path of this.paths_) {
      file_info_list.push(new blink.mojom.FileChooserFileInfo({
          nativeFile: {
              filePath: {path: path},
              displayName: {data:[]}
          }
      }));
    }
    resolve({result: {files: file_info_list}});
  }
}

window.mockFileChooserFactory = new MockFileChooserFactory();

})();

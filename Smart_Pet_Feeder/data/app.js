// selectors
const menuToggle = document.querySelector(".menu-toggle input");
const nav = document.querySelector("nav ul");
const confirmRestart = document.querySelector("#confirmRestartButton");
const dialogBox = document.querySelector("#dialogBoxBg");
const cancelButton = document.querySelector(".cancelButton");
const restartButton = document.querySelector("#restartButton");

// eventListeners
menuToggle.addEventListener("click", function () {
  nav.classList.toggle("slide");
});
nav.addEventListener("click", function () {
  nav.classList.remove("slide");
  document.getElementById("checkBox").checked = false;
});

confirmRestart.addEventListener("click", function () {
  dialogBox.classList.add("visible");
  dialogBox.classList.remove("hidden");
});

cancelButton.addEventListener("click", function () {
  dialogBox.classList.remove("visible");
  dialogBox.classList.add("hidden");
});

restartButton.addEventListener("click", function () {
  window.close();
});

setInterval(function () {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function () {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("weight").innerHTML = this.responseText;
      console.log(this);
    }
  };
  xhttp.open("GET", "/weight", true);
  xhttp.send();
}, 1000);

setInterval(function () {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function () {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("percentage").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/percentage", true);
  xhttp.send();
}, 10000);

setInterval(function () {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function () {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("last").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/last", true);
  xhttp.send();
}, 10000);

setInterval(function () {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function () {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("mode").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/mode", true);
  xhttp.send();
}, 10000);

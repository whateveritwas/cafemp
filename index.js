var countryElements = document.getElementById('countries').childNodes;
var countryCount = countryElements.length;
const countrys = ["Poland", "Slovenia", "Bosnia and Herzegovina", "Estonia", "Latvia", "Lithuania", "Ukraine", "Germany"];
var click = 0;

function openPodcastMenu() {
	document.getElementById("markdown-list").innerHTML = '<div class="loading loading03">Laden...</div>';
	loadMarkdownToList('content/infoCards/Podcasts.md');
	getCountryInfo("Podcasts");
}

function closeInfoCard() {
	document.getElementById("infoCard").style.display="none";
}

function translateCountryNames(country) {
	const names = {
		"Poland": "Polen",
		"Slovenia": "Slovenien",
		"Bosnia and Herzegovina": "Bosnien und Herzegowina", 
		"Estonia": "Estland",
		"Latvia": "Lettland",
		"Lithuania": "Litauen",
		"Ukraine": "Ukraine",
		"Germany": "Über"
	}
	
	return names[country] || country;
}

function getCountryInfo(country) {
	const heading = document.getElementById("infoCardHeading");
	heading.innerHTML = translateCountryNames(country);
	document.getElementById("infoCard").style.display = "block";

	history.pushState({ type: 'infoCard' }, '');
}

document.querySelectorAll('.collapsible').forEach(header => {
	header.addEventListener('click', () => {
		const content = header.nextElementSibling;
		content.style.display = content.style.display === 'block' ? 'none' : 'block';
	});
});

function stripMarkdown(md) {
  return md
    .replace(/(\*\*|__)(.*?)\1/g, '$2')   		// bold
    .replace(/(\*|_)(.*?)\1/g, '$2')      		// italic
    .replace(/`([^`]+)`/g, '$1')          		// inline code
    .replace(/!\[.*?\]\(.*?\)/g, '')      		// images
    .replace(/\[([^\]]+)\]\([^\)]+\)/g, '$1') 	// links
    .replace(/[#>*\-+~`]/g, '')            		// other markdown chars
    .trim();
}

async function loadMarkdownToList(url) {
	const res = await fetch(url);
	const mdText = await res.text();

	const fullHTML = marked.parse(mdText);

	const tempDiv = document.createElement('div');
	tempDiv.innerHTML = fullHTML;

	const headings = tempDiv.querySelectorAll('h2');

	const list = document.getElementById("markdown-list");
	list.innerHTML = '';

	headings.forEach((heading, i) => {
	const li = document.createElement('li');

	const h3 = document.createElement('h3');
	h3.className = 'collapsible';
	h3.textContent = heading.textContent;
	let contentNodes = [];
	let next = heading.nextSibling;
	while (next && !(next.nodeType === 1 && next.tagName === 'H2')) {
		contentNodes.push(next);
		next = next.nextSibling;
	}

	const contentDiv = document.createElement('div');
	contentDiv.className = 'content';
	contentNodes.forEach(node => contentDiv.appendChild(node));

	h3.addEventListener('click', () => {
		const isVisible = contentDiv.style.display === 'block';
		contentDiv.style.display = isVisible ? 'none' : 'block';
		h3.classList.toggle('open', !isVisible);

		const arrowSpan = h3.querySelector('.arrow');
		if (arrowSpan) {
			arrowSpan.textContent = isVisible ? '▶' : '▼';
		}

		if (!isVisible) {
			history.pushState({ type: 'collapsible', id: h3.textContent }, '');
		}
	});

	li.appendChild(h3);
	li.appendChild(contentDiv);
	list.appendChild(li);
	});
}

for (var i = 0; i < countryCount; i++) {
	if (countrys.includes(countryElements[i].getAttribute('data-name'))) {
		countryElements[i].style.fill = "SeaGreen";
		
		countryElements[i].onclick = function() {
			document.getElementById("markdown-list").innerHTML = '<div class="loading loading03">Laden...</div>';
			loadMarkdownToList('content/infoCards/' + this.getAttribute('data-name') + '.md');
			getCountryInfo(this.getAttribute('data-name'));
		}
	}
	if (countryElements[i].getAttribute('data-name') === "Spain") {
		countryElements[i].onclick = function() {
			if(click++ === 2) this.style.display = "none"; //🥚
		}
	}
}

window.addEventListener('popstate', (event) => {
	const infoCard = document.getElementById("infoCard");

	// Priority 1: close open collapsibles
	const openSections = Array.from(document.querySelectorAll('.content'))
		.filter(div => div.style.display === 'block');

	if (openSections.length > 0) {
		openSections.forEach(div => {
			div.style.display = 'none';
			
			// Also remove 'open' class from the header
			const header = div.previousElementSibling;
			if (header && header.classList.contains('collapsible')) {
				header.classList.remove('open');

				// Reset the arrow if it exists
				const arrowSpan = header.querySelector('.arrow');
				if (arrowSpan) {
					arrowSpan.textContent = '▶';
				}
			}
		});
		return; // Stop here, don't close the infoCard yet
	}

	// Priority 2: close info card if open
	if (infoCard.style.display === 'block') {
		infoCard.style.display = 'none';
		return;
	}

	// Otherwise: let browser handle default back navigation
});

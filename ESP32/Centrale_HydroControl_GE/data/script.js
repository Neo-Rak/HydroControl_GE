document.addEventListener('DOMContentLoaded', function() {

    const nodesTbody = document.getElementById('nodes-tbody');
    const reservoirSelect = document.getElementById('reservoir_select');
    const wellSelect = document.getElementById('well_select');
    const assignForm = document.getElementById('assign-form');
    const assignStatus = document.getElementById('assign-status');

    function updateDashboard() {
        fetch('/api/status')
            .then(response => response.json())
            .then(data => {
                // Vider les contenus actuels
                nodesTbody.innerHTML = '';
                reservoirSelect.innerHTML = '';
                wellSelect.innerHTML = '';

                // Remplir la table des noeuds
                data.nodes.forEach(node => {
                    let row = nodesTbody.insertRow();

                    let nameCell = row.insertCell(0);
                    nameCell.innerHTML = node.name ? `<strong>${node.name}</strong>` : '<i>Non défini</i>';

                    row.insertCell(1).textContent = node.id;
                    row.insertCell(2).textContent = node.type;
                    row.insertCell(3).textContent = node.status;
                    row.insertCell(4).textContent = node.rssi;
                    row.insertCell(5).textContent = new Date(node.lastSeen).toLocaleTimeString();

                    let actionCell = row.insertCell(6);
                    let editButton = document.createElement('button');
                    editButton.textContent = 'Éditer';
                    editButton.onclick = function() { editNodeName(node.id); };
                    actionCell.appendChild(editButton);

                    // Remplir les menus déroulants (avec le nom si disponible)
                    let displayName = node.name && node.name.length > 0 ? `${node.name} (${node.id})` : node.id;
                    if (node.type === 'AquaReservPro') {
                        let option = new Option(displayName, node.id);
                        reservoirSelect.add(option);
                    } else if (node.type === 'WellguardPro') {
                        let option = new Option(displayName, node.id);
                        wellSelect.add(option);
                    }
                });
            })
            .catch(error => console.error('Error fetching status:', error));
    }

    // Gérer la soumission du formulaire d'assignation
    assignForm.addEventListener('submit', function(event) {
        event.preventDefault();
        const formData = new FormData(assignForm);

        fetch('/api/assign', {
            method: 'POST',
            body: new URLSearchParams(formData)
        })
        .then(response => response.text())
        .then(text => {
            assignStatus.textContent = text;
            setTimeout(() => assignStatus.textContent = '', 3000); // Effacer le message après 3s
        })
        .catch(error => {
            assignStatus.textContent = 'Erreur: ' + error;
        });
    });

    // Mettre à jour le tableau de bord toutes les 2 secondes
    setInterval(updateDashboard, 2000);
    updateDashboard(); // Premier appel
});

function editNodeName(nodeId) {
    const newName = prompt("Entrez le nouveau nom pour le noeud " + nodeId + ":");
    if (newName === null || newName.trim() === '') {
        return; // Annulé ou vide
    }

    fetch('/api/set-name', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: nodeId, name: newName })
    })
    .then(response => {
        if (response.ok) {
            // Mettre à jour le nom directement dans la table pour un retour visuel immédiat
            let table = document.getElementById('nodes-table');
            for (let i = 1; i < table.rows.length; i++) {
                if (table.rows[i].cells[1].textContent === nodeId) {
                    table.rows[i].cells[0].innerHTML = `<strong>${newName}</strong>`;
                    break;
                }
            }
        } else {
            alert("Erreur lors de la mise à jour du nom.");
        }
    })
    .catch(error => console.error('Error setting node name:', error));
}

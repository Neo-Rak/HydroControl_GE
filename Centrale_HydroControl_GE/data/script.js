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
                    row.insertCell(0).textContent = node.id;
                    row.insertCell(1).textContent = node.type;
                    row.insertCell(2).textContent = node.status;
                    row.insertCell(3).textContent = node.rssi;
                    row.insertCell(4).textContent = new Date(node.lastSeen).toLocaleTimeString();

                    // Remplir les menus déroulants
                    if (node.type === 'AquaReservPro') {
                        let option = new Option(node.id, node.id);
                        reservoirSelect.add(option);
                    } else if (node.type === 'WellguardPro') {
                        let option = new Option(node.id, node.id);
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
